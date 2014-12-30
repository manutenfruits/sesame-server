
(function () {
  'use strict';

  var $ = document.querySelector.bind(document);

  var navdrawerContainer = $('.os-aside'),
      header = $('.os-header'),
      menuBtn = $('.menu-button'),
      main = $('.os-main'),
      buttons = $('.button-wrapper');

  var password = 'H4fr1Kah!',
      host = '10.0.1.237',
      nonce = 0;

  var ajax = function (method, url, cb) {
    var xmlhttp;

    if (window.XMLHttpRequest) {
      // code for IE7+, Firefox, Chrome, Opera, Safari
      xmlhttp = new XMLHttpRequest();
    } else {
      // code for IE6, IE5
      xmlhttp = new ActiveXObject('Microsoft.XMLHTTP');
    }

    xmlhttp.onreadystatechange = function() {
      if (xmlhttp.readyState === 4 ) {
        cb(xmlhttp.status, xmlhttp.responseText);
      }
    };

    xmlhttp.open(method, url, true);
    xmlhttp.send();
  };

  var openDoor = function (door) {
    var hash = CryptoJS.HmacSHA256(String(nonce), password);

    var url = ['http://',
        host, '/',
        nonce, '/',
        door, '/',
        hash].join('');

    ajax('POST', url, function (status, response) {
      switch (status) {
        case 200:
          nonce++;
          console.log('Door opened');
          break;
        case 401:
          console.log('Bad password');
          break;
        case 400:
          nonce = Number(response);
          openDoor(door);
          break;
      }
    });
  };

  var closeMenu = function () {
    header.classList.remove('nav-opened');
  };

  var toggleMenu = function () {
    header.classList.toggle('nav-opened');
  };

  main.addEventListener('click', closeMenu);
  menuBtn.addEventListener('click', toggleMenu);
  navdrawerContainer.addEventListener('click', function (event) {
    if (event.target.nodeName === 'A' || event.target.nodeName === 'LI') {
      closeMenu();
    }
  });

  buttons.addEventListener('click', function (e) {
    if (e.target.classList.contains('door-opener')) {
      var doorNr = e.target.dataset.door;

      openDoor(doorNr);
    }
  });

  var showMessage = (function () {
    var wrapper = $('message-panel-wrapper'),
        message = $('message-panel');

    return function (text) {

    };

  })();


})();
