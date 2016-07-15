// Copyright(c) 2016 Anseok Lee (anseok83_at_gmail_dot_com)
// MIT License
//
// THD (Temperature, humidity and dust density) Server
// https://github.com/anseok83/thdm

// Version 0.0.2 (2016-05-21)

// Requires
var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var MongoClient = require('mongodb').MongoClient
var assert = require('assert');
var ObjectId = require('mongodb').ObjectID;

// Mongodb URL
var url = 'mongodb://DB_ACCOUNT:DB_PASSWORD@ds000000.mlab.com:00000/DB_NAME';

// THD web page for monitoring
app.get('/', function(req, res){
  res.sendFile(__dirname + '/thdgc.html');
});

// Receive the reported sensor data from THD Sensor node
io.on('connection', function(socket){
  console.log('a THD Sensor connected');

  // Event to receive data
  socket.on('thd', function(thdJson){
    var thdStr = JSON.stringify(thdJson);
    console.log('thdValue: ' + thdStr);
    var thdObj = JSON.parse(thdStr);

    // datatime
    datetimeStr = new Date().toISOString();

    // Log
    console.log('[' + datetimeStr + '] ' + 'temp: ' + thdObj.temp + 'humid: ' + thdObj.humid + 'dust: ' + thdObj.dust + 'eth: ' + thdObj.eth);

    // Insert to mongodb
    MongoClient.connect(url, function(err, db) {
      if (err) {
        console.log('ignore db error');
        return;
      }

      var tempVal = Number(thdObj.temp);
      var humidVal = Number(thdObj.humid);
      var dustVal = Number(thdObj.dust);
      var ethVal = String(thdObj.eth);
      
      // Insert data to mongodb
      db.collection('thd_data').insert({temp:tempVal, humid:humidVal, dust:dustVal, eth:ethVal, timestamp:datetimeStr}, function(err, doc){
        console.log('inserted');

        // Close the connection
        db.close();
      });
      
    });
  });
  socket.on('disconnect', function(){
    console.log('user disconnected');
  });
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
