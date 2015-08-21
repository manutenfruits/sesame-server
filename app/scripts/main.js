
(function() {
  'use strict';

  var $ = document.querySelector.bind(document);
  var $$ = document.querySelectorAll.bind(document);

  var header = $('.os-header');
  var menuBtn = $('.menu-button');
  var main = $('.os-main');
  var settingsForm = $('#settings');
  var settingsHost = $('#door-host');
  var settingsPasswd = $('#door-passwd');
  var buttons = $$('.door-button');

  var settings = {};

  var ajax = function(method, url, cb) {
    var xmlhttp;

    if (window.XMLHttpRequest) {
      // code for IE7+, Firefox, Chrome, Opera, Safari
      xmlhttp = new XMLHttpRequest();
    } else {
      // code for IE6, IE5
      xmlhttp = new ActiveXObject('Microsoft.XMLHTTP');
    }

    xmlhttp.onreadystatechange = function() {
      if (xmlhttp.readyState === 4) {
        var response = xmlhttp.responseText;
        if (response.length &&
            xmlhttp.getResponseHeader('Content-Type') === 'application/json') {
          response = JSON.parse(response);
        }
        cb(xmlhttp.status, response);
      }
    };

    xmlhttp.open(method, url, true);
    xmlhttp.send();
  };

  var openDoor = function(door, cb, cberr) {
    var s = settings;
    var hash = CryptoJS.HmacSHA256(String(s.nonce), s.password);
    var url = [
          s.host, '/',
          s.nonce, '/',
          door, '/',
          hash].join('');

    ajax('POST', url, function(status, response) {
      var success;
      switch (status) {
      case 200:
        s.nonce++;
        success = true;
        console.info('Door opened');
        break;
      case 401:
        success = false;
        console.error('Bad password');
        break;
      case 400:
        s.nonce = response.nonce;
        openDoor(door, cb, cberr);
        break;
      }

      if (success !== undefined) {
        var fn = success ? cb : cberr;
        if (typeof fn === 'function') {
          fn(status, response);
        }
      }
    });
  };

  main.addEventListener('click', function() {
    header.classList.remove('nav-opened');
  });

  menuBtn.addEventListener('click', function() {
    header.classList.toggle('nav-opened');
  });

  main.addEventListener('click', function(e) {
    if (e.target.classList.contains('door-button')) {
      var button = e.target;
      var doorNr = button.dataset.door;

      if (doorNr) {
        //Disable all other buttons
        toggleButtons(false, doorNr);

        button.dataset.state = 'waiting';

        openDoor(doorNr,
          function() {
            button.dataset.state = 'success';
            resetButton(button);
          }, function() {
            button.dataset.state = 'error';
            resetButton(button);
          });
      }
    }
  });

  //Revert button state after 1 second
  var resetButton = function(button) {
    setTimeout(function() {
      delete button.dataset.state;
      toggleButtons(true);
    }, 1000);
  };

  //Disables all buttons except the one with the doorNr specified
  var toggleButtons = function(enable, exception) {
    [].forEach.call(buttons, function(button) {
      if (exception === undefined ||
          button.dataset.door !== exception) {
        button.disabled = !enable;
      }
    });
  };

  settingsForm.addEventListener('submit', function(e) {
    e.preventDefault();

    var host = settings.host = settingsHost.value;
    var password = settings.password = settingsPasswd.value;

    localStorage.setItem('host', host);
    localStorage.setItem('password', password);

    //Close aside
    header.classList.remove('nav-opened');
    toggleButtons(false);
    testConnection();
  });

  //Init
  var loadSettings = function() {
    var host = localStorage.getItem('host');
    var password = localStorage.getItem('password');

    if (!!host) {
      settingsHost.value = settings.host = host;
    }

    if (!!password) {
      settingsPasswd.value = settings.password = password;
    }

    settings.nonce = 0;
  };

  var testConnection = function() {
    var s = settings;

    if (!!s.host && !!s.password) {
      ajax('OPTIONS', settings.host, function(status) {
        toggleButtons(status === 200);
      });
    } else {
      toggleButtons(false);
    }
  };

  loadSettings();
  testConnection();

})();
