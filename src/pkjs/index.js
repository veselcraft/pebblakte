var initialized = false;
var Clay = require('pebble-clay');
var options = {
        "token": "",
        "domain": "ovk.to",
        "messages": ["Да.", 
          "Нет.",
          "Окей.",
          "Подумаю.",
          "Ага."
        ],
        "notifications": 0
      };
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);

var seq = 0;
var error_count = 0;
var max_errors = 5;
var max_answers = 10;
var optimal_msg_count = 5;
var max_msg_parts = 10;
var max_msg_parts_total = 50;
var max_source_length = 30;
var max_who_length = 50;
var lastRecvSeq = 0;
var requestTimeout = 10000;
var timeout;

function startSend()
{
  sending = true;
  //console.log("Sending: "+ JSON.stringify(queue[0]));
  timeout=setTimeout(sendTimeout, 10000);
  Pebble.sendAppMessage(queue[0], sendOk, sendError);
}

function sendOk(e)
{
  //console.log("Sending ok");
  clearTimeout(timeout);
  error_count = 0;
  queue.splice(0, 1);
  if (queue.length > 0)
  {
    startSend();
  } else {
    sending = false;
  }
}

function sendError(e)
{
  //console.warn("Sending error");
  clearTimeout(timeout);
  error_count++;
  if (error_count < max_errors)
  {
    startSend();
  } else {
    console.warn("Too many errors");
  }
}

function sendTimeout()
{
  //console.log("Sending timeout");
  error_count++;
  if (error_count < max_errors)
  {
    startSend();
  } else {
    console.warn("Too many timeouts");
  }
}

function send(data)
{
  data["MSG_KEY_SEQ"] = seq++;
  queue.push(data);
  if (!sending) startSend();
}

function downloadDialogs()
{
  send({"MSG_KEY_STATE": 1});
  var response;
  var req = new XMLHttpRequest();  
  var url = "https://"+options.domain+"/method/messages.getConversations?extended=1&access_token="+options.token;
  req.open('GET', url, true);
  req.onreadystatechange = function() {
    if (req.readyState == 4) {
      if(req.status == 200) {
        //console.log(req.responseText);
        response = JSON.parse(req.responseText);            
        if ("error" in response != 0) {
          send({"MSG_KEY_VK_ERROR": response.error.error_code});
          console.warn("Error: "+JSON.stringify(response));
        } else {
          send({"MSG_KEY_STATE": 2, "MSG_KEY_DIALOGS_START": 0, "MSG_KEY_SHOW_NOTIFICATIONS": options.notifications});
          response = response.response;
          for (var d = 0; d < response.items.length; d++) {
            var id, out, title, text;
            out = response.items[d].out;
            id = response.items[d].conversation.peer.id;
            text = response.items[d].last_message.body;
            
            title = response.profiles[d].first_name + " " + response.profiles[d].last_name;
            if (response.items[d].attachments && (response.items[d].attachments.length > 0))
              text = text + " "+JSON.stringify(response.items[d].attachments);
            text = text.replace(/\[id.+\|(.+)\]/,"$1");
            text = text.trim();
            if (title.length > 30) title = title.substring(0, 30);
            if (text.length > 30) text  = text.substring(0, 30);
            
            send({"MSG_KEY_DIALOG_ID": id, "MSG_KEY_DIALOG_OUT": out});
            send({"MSG_KEY_DIALOG_TITLE": title});
            send({"MSG_KEY_DIALOG_TEXT": text});
          }
          send({"MSG_KEY_DIALOG_END": 1});
        }
      } else {
        console.warn("HTTP error: "+req.status);
        send({"MSG_KEY_CRITICAL_ERROR": "Error: "+req.status});
      }
    }
  }
  req.timeout = requestTimeout;
  req.ontimeout = function () {
    console.warn("HTTP error: timeout");  
    send({"MSG_KEY_CRITICAL_ERROR": "HTTP timeout"});
  }
  req.send(null);
}

function downloadMessages(user_id)
{
  send({"MSG_KEY_STATE": 1});
  var response;
  var req = new XMLHttpRequest();  
  var url = "https://"+options.domain+"/method/messages.getHistory?user_id="+user_id+"&count=10&extended=1&access_token="+options.token;
  req.open('GET', url, true);
  req.onreadystatechange = function() {
    if (req.readyState == 4) {
      if(req.status == 200) {
        //console.log(req.responseText);
        response = JSON.parse(req.responseText);            
        if ("error" in response) {
          send({"MSG_KEY_VK_ERROR": response.error.error_code});
          console.warn("Error: "+JSON.stringify(response));
        } else {
          send({"MSG_KEY_STATE": 2, "MSG_KEY_MESSAGES_START": 0});
          response = response.response;
          var offset = new Date().getTimezoneOffset()*60;
          var totalParts = 0;
          //for (var d = response.length-1; d >= 0; d--) {
          for (var d = 0; d < response.count; d++) {
            if (totalParts >= max_msg_parts_total)
            {
              console.warn("Messages are too big, stop");
              continue;
            }
            var name, out, text, date, attachmets;
            out = response.items[d].out;
            text = response.items[d].text;
            date = response.items[d].date;
            read = response.items[d].read_state;

            if (out == 0) {
              name = response.profiles[1].first_name;
            } else {
              name = response.profiles[0].first_name;
            }
            // Если сообщений слишком много, и они все прочитаны, то не грузим
            if ((d >= optimal_msg_count) && ((read != 0) || (out != 0)))
              break;
            
            started = true;
            
            if (out > 0) read = 1;
            text = text.replace(/\[id.+\|(.+)\]/,"$1");
            if (response.items[d].attachments && (response.items[d].attachments.length > 0))
            {
              if (text.length > 0) text += "\r\n";
              text = text + "Вложения: "+JSON.stringify(response.items[d].attachments);
            }
            var textParts = text.match(/[\s\S]{1,32}/g);
            for (var p = 0; (textParts != null) && (p < textParts.length) && p < max_msg_parts; p++) {
              send({"MSG_KEY_MESSAGES_TEXT": textParts[p]});
              totalParts++;
              if (p == max_msg_parts-1 && p < textParts.length-1) send({"MSG_KEY_MESSAGES_TEXT": "..."});
            }
            if (name.length > 20) name  = name.substring(0, 20);
            send({"MSG_KEY_MESSAGES_OUT": out, "MSG_KEY_MESSAGES_DATE": (date-offset), "MSG_KEY_MESSAGES_READ": read, "MSG_KEY_MESSAGES_NAME": name});
          }
          send({"MSG_KEY_MESSAGES_END": user_id});
        }
      } else {
        console.warn("HTTP error: "+req.status);
        send({"MSG_KEY_CRITICAL_ERROR": "Error: "+req.status});
      }
    }
  }
  req.timeout = requestTimeout;
  req.ontimeout = function () {
    console.warn("HTTP error: timeout");  
    send({"MSG_KEY_CRITICAL_ERROR": "HTTP timeout"});
  }
  req.send(null);
}

function downloadNotifications()
{
  send({"MSG_KEY_STATE": 1});
  var response;
  var req = new XMLHttpRequest();  
  var url = "https://api.vk.com/method/execute.getNotifications?v=5.81&access_token="+options.token;
  req.open('GET', url, true);
  req.onreadystatechange = function() {
    if (req.readyState == 4) {
      if(req.status == 200) {
        //console.log(req.responseText);
        response = JSON.parse(req.responseText);            
        if ("error" in response) {
          send({"MSG_KEY_VK_ERROR": response.error.error_code});
          console.warn("Error: "+JSON.stringify(response));
        } else {
          send({"MSG_KEY_STATE": 2, "MSG_KEY_NOTIFICATIONS_START": 0});
          response = response.response;
          for (var d = response.length-1; d >= 0; d--) {
            var type, who, whocount, text, attachments, source, sattachments, sex;
            type = response[d].type;
            who = response[d].who;
            whocount = response[d].whocount;
            text = response[d].text;
            attachments = response[d].attachments;
            source = response[d].source;
            sattachments = response[d].sattachments;
            sex = response[d].sex;
            
            if (text)
            {
              text = text.replace(/\[id.+\|(.+)\]/,"$1");
            }
            if (attachments && attachments.length > 0)
            {
              if (text && text.length > 0) text = text + "\r\n";
              text += "Вложения: " + JSON.stringify(attachments);
            }          

            if (source)
            {
              source = source.replace(/\[id.+\|(.+)\]/,"$1");              
            }
            if (sattachments && sattachments.length > 0)
            {
              if (source && source.length > 0) source = source + "\r\n";
              source += JSON.stringify(sattachments);
            }
            if (source && source.length > max_source_length)
            {
              source = source.substring(0,max_source_length) + "...";
            }
            
            if (who && who.length > 0) who = who.replace(" u ", " и ");
            if (who && who.length > max_who_length)
            {
              who = who.substring(0, max_who_length-10) + "... ("+whocount+" чел.)";              
            }
            
            sendNotification(type, who, text, source, sex);
            
          }
          send({"MSG_KEY_NOTIFICATIONS_END": 1});
        }
      } else {
        console.warn("HTTP error: "+req.status);
        send({"MSG_KEY_CRITICAL_ERROR": "Error: "+req.status});
      }
    }
  }
  req.timeout = requestTimeout;
  req.ontimeout = function () {
    console.warn("HTTP error: timeout");  
    send({"MSG_KEY_CRITICAL_ERROR": "HTTP timeout"});
  }
  req.send(null);
}

function sendNotification(type, who, text, source, sex)
{
  var result = "";
  
  switch (type)
  {
    case "follow":
      result = "На ваши обновления подписал";
      switch (sex)
      {
        case 0:
          result += "ись ";
          break;
        case 1:
          result += "ась ";
          break;
        case 2:
          result += "ся ";
          break;
      }
      result += who;
      break;
  
    case "friend_accepted":
      switch (sex)
      {
        case 0:
          result = "Приняли заявку в друзья: " + who;
          break;
        case 1:
          result = who + " приняла заявку в друзья."
          break;
        case 2:
          result = who + " принял заявку в друзья."
          break;
      }
      break;
      
    case "mention":
      result = who;
      switch (sex)
      {
        case 0:
          result += " упомянуло"
          break;
        case 1:
          result += " упомянула"
          break;
        case 2:
          result += " упомянул"
          break;
      }
      result += " вас в посте:"+"\r\n"+source;
      break;

    case "mention_comments":
      result = who;
      switch (sex)
      {
        case 0:
          result += " упомянуло"
          break;
        case 1:
          result += " упомянула"
          break;
        case 2:
          result += " упомянул"
          break;
      }
      result += " вас в комментарии:"+"\r\n"+text;      
      break;
      
    case "wall":
      result = who;
      switch (sex)
      {
        case 0:
          result += " написало"
          break;
        case 1:
          result += " написала"
          break;
        case 2:
          result += " написал"
          break;
      }
      result += " на вашей стене:"+"\r\n"+text;
      break;

    case "comment_post":
      result = who;
      switch (sex)
      {
        case 0:
          result += " ответило"
          break;
        case 1:
          result += " ответила"
          break;
        case 2:
          result += " ответил"
          break;
      }
      if (source && (source.length > 0))
      {
        result += ' на ваш пост "'+source + '":\r\n' + text;
      } else {
        result += " на ваш пост"+":\r\n" + text;
      }
      break;

    case "comment_photo":
      result = who;
      switch (sex)
      {
        case 0:
          result += " прокомментировало";
          break;
        case 1:
          result += " прокомментировала";
          break;
        case 2:
          result += " прокомментировал";
          break;
      }
      result += " ваше фото:"+"\r\n" + text;
      break;

    case "comment_video":
      result = who;
      switch (sex)
      {
        case 0:
          result += " прокомментировало";
          break;
        case 1:
          result += " прокомментировала";
          break;
        case 2:
          result += " прокомментировал";
          break;
      }
      result += " ваше видео:"+"\r\n" + text;
      break;
      
    case "reply_comment":
      result = who;
      switch (sex)
      {
        case 0:
          result += " ответило";
          break;
        case 1:
          result += " ответила";
          break;
        case 2:
          result += " ответил";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' на ваш комментарий "' + source + '":\r\n' + text;
      } else {
        result += " на ваш комментарий:"+"\r\n" + text;
      }
      break;
      
    case "reply_comment_photo":
      result = who;
      switch (sex)
      {
        case 0:
          result += " ответило";
          break;
        case 1:
          result += " ответила";
          break;
        case 2:
          result += " ответил";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' на ваш комментарий к фото "' + source + '":\r\n' + text;
      } else {
        result += " на ваш комментарий к фото:"+"\r\n" + text;
      }
      break;
      
    case "reply_comment_video":
      result = who;
      switch (sex)
      {
        case 0:
          result += " ответило";
          break;
        case 1:
          result += " ответила";
          break;
        case 2:
          result += " ответил";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' на ваш комментарий к видео "' + source + '":\r\n' + text;
      } else {
        result += " на ваш комментарий к видео:"+"\r\n" + text;
      }
      break;
      
    case "reply_topic":
      result = who;
      switch (sex)
      {
        case 0:
          result += " ответило";
          break;
        case 1:
          result += " ответила";
          break;
        case 2:
          result += " ответил";
          break;
      }
      result += " в обсуждении:"+"\r\n" + text;
      break;

    case "like_post":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' пост "'+source+'".';
      } else {
        result += ' ваш пост.';
      }
      break;

    case "like_comment":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' комментарий "'+source+'".';
      } else {
        result += ' ваш комментарий.';
      }
      break;
      
    case "like_comment_photo":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' комментарий к фото "'+source+'".';
      } else {
        result += ' ваш комментарий к фото.';
      }
      break;

    case "like_comment":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' комментарий к видео "'+source+'".';
      } else {
        result += ' ваш комментарий.';
      }
      break;

      case "like_photo":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      result += ' ваше фото.';
      break;

    case "like_video":
      result = who;
      switch (sex)
      {
        case 0:
          result += " лайкнули";
          break;
        case 1:
          result += " лайкнула";
          break;
        case 2:
          result += " лайкнул";
          break;
      }
      result += ' ваше видео.';
      break;
      
    case "copy_post":
      result = who;
      switch (sex)
      {
        case 0:
          result += " рассказали";
          break;
        case 1:
          result += " рассказала";
          break;
        case 2:
          result += " рассказал";
          break;
      }
      if (source && source.length > 0)
      {
        result += ' про ваш пост "'+source+'".';
      } else {
        result += ' про ваш пост.';
      }
      break;
      
    case "copy_photo":
      result = who;
      switch (sex)
      {
        case 0:
          result += " рассказали";
          break;
        case 1:
          result += " рассказала";
          break;
        case 2:
          result += " рассказал";
          break;
      }
      result += ' про ваше фото.';
      break;

    case "copy_video":
      result = who;
      switch (sex)
      {
        case 0:
          result += " рассказали";
          break;
        case 1:
          result += " рассказала";
          break;
        case 2:
          result += " рассказал";
          break;
      }
      result += ' про ваше видео.';
      break;
      
    default:
      result = "Неизвестное событие: "+type;
      break;
  }
  
  var textParts = result.match(/[\s\S]{1,32}/g);
  for (var p = 0; (textParts != null) && (p < textParts.length) && p < max_msg_parts; p++) {
    send({"MSG_KEY_NOTIFICATIONS_TEXT": textParts[p]});
    if (p == max_msg_parts-1 && p < textParts.length-1) send({"MSG_KEY_NOTIFICATIONS_TEXT": "..."});
  }
  send({"MSG_KEY_NOTIFICATIONS_TEXT_END": 1});
}

function sendList() {
  send({"MSG_KEY_ANSWERS_START": 0});
  for (var m = 0; (m < options.messages.length) && (m < max_answers); m++) {
    var msg = options.messages[m];
    if (msg.length > 30) msg = msg.substring(0, 30);
    send({"MSG_KEY_ANSWERS_ELEMENT": msg});
  }
  send({"MSG_KEY_ANSWERS_END": 1});
}

function answer(user_id, text) {
  var response;
  var req = new XMLHttpRequest();  
  if (user_id > 0)
  {
    var url = "https://"+options.domain+"/method/messages.send?user_id="+user_id+"&access_token="+options.token+"&message="+encodeURIComponent(text);
  } else {
    var url = "https://"+options.domain+"/method/messages.send?chat_id="+(-user_id)+"&access_token="+options.token+"&message="+encodeURIComponent(text);
  }
  req.open('GET', url, true);
  req.onreadystatechange = function() {
    if (req.readyState == 4) {
      if(req.status == 200) {
        //console.log(req.responseText);
        response = JSON.parse(req.responseText);            
        if ("error" in response) {
          send({"MSG_KEY_VK_ERROR": response.error.error_code});
          console.warn("Error: "+response.error.error_msg);
        } else {
          downloadMessages(user_id);
          //downloadDialogs();
        }
      } else {
        console.warn("HTTP error: "+req.status);
        send({"MSG_KEY_CRITICAL_ERROR": "Error: "+req.status});
      }
    }
  }
  req.timeout = requestTimeout;
  req.ontimeout = function () {
    console.warn("HTTP error: timeout");  
    send({"MSG_KEY_CRITICAL_ERROR": "HTTP timeout"});
  }
  req.send(null);  
}
      
Pebble.addEventListener("ready", function() {
  initialized = true;
  queue = [];
  sending = false;
//  console.log("VKontakte - started.");
  var json = window.localStorage.getItem('vkontakte-config');  
  if (typeof json === 'string') {
    try {
      options = JSON.parse(json);
      //console.log("Loaded stored config: " + json);
    } catch(e) {
      console.warn("Stored config json parse error: " + json + ' - ' + e);
    }
  }
  send({"MSG_KEY_JAVASCRIPT_STARTED": 0});  
  if (options.token.length > 0)
    downloadDialogs();
  else
    send({"MSG_KEY_CRITICAL_ERROR": "NOTOKEN"});
});

Pebble.addEventListener("showConfiguration", function() {
  clay.config = clayConfig;
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (e && !e.response) { return; }
  
  var dict = clay.getSettings(e.response);

  var dictKeys = Object.keys(dict);
  var newDict = {}; // интересно а кто нибудь эту переменную обзывал членом

  newDict.token = dict["1000"];
  newDict.domain = dict["1001"];
  newDict.notifications = dict["1002"];
  let msgs = dict["1003"];
  newDict.messages = msgs.split("; ");

  options = newDict; // тут я случайно написал Dick вместо Dict
  window.localStorage.setItem('vkontakte-config', JSON.stringify(newDict));

  if (newDict.token.length > 0)
    downloadDialogs();
});

Pebble.addEventListener("appmessage",
  function(e) {
    //console.log("Received message: " + JSON.stringify(e.payload));    
    if ("MSG_KEY_SEQ" in e.payload)
    {
      var seq = e.payload["MSG_KEY_SEQ"];
      if (seq == lastRecvSeq)
      {
        console.warn("Received duplicate message");
        return;
      }
      lastRecvSeq = seq;
    }
    
    if ("MSG_KEY_REFRESH_DIALOGS" in e.payload)
    {
      downloadDialogs();
    }
    if ("MSG_KEY_REQUEST" in e.payload)
    {
      var id = e.payload["MSG_KEY_REQUEST"];
      if (id != 0)
        downloadMessages(id);
      else
        downloadNotifications();
    }
    if ("MSG_KEY_REQUEST_LIST" in e.payload)
    {
      sendList();
    }
    if (("MSG_KEY_ANSWER_TO" in e.payload) && ("MSG_KEY_ANSWER_MESSAGE" in e.payload))
    {
      var id = e.payload["MSG_KEY_ANSWER_TO"];
      var msg = e.payload["MSG_KEY_ANSWER_MESSAGE"];
      answer(id, options.messages[msg]);
    }
    if (("MSG_KEY_ANSWER_TO" in e.payload) && ("MSG_KEY_ANSWER_MESSAGE_CUSTOM" in e.payload))
    {
      var id = e.payload["MSG_KEY_ANSWER_TO"];
      var msg = e.payload["MSG_KEY_ANSWER_MESSAGE_CUSTOM"];
      answer(id, msg);
    }
  }
);
