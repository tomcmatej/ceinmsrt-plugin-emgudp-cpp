clear all; close all;
%% matlab UDP code

% UDP Server IP and Port
serverIP = '127.0.0.1';  % Replace with the actual IP of your UDP server
serverPort = 31000;      % Replace with the actual port of your UDP server

% Create UDP object
udpObject = udp(serverIP, serverPort, 'LocalPort', 54321, 'Terminator', '');

% Open the UDP connection
fopen(udpObject);

%% create sine wave
% Parameters
amplitude1 = 1;          % Amplitude of the sine wave
frequency1 = 2;          % Frequency of the sine wave (in Hz)
amplitude2 = 1;          % Amplitude of the sine wave
frequency2 = 7;          % Frequency of the sine wave (in Hz)
duration = 20;           % Duration of the sine wave (in seconds)
samplingRate = 1000;    % Sampling rate (number of samples per second)

% Time vector
t = 0:1/samplingRate:duration;


for it = 1:length(t)
% Generate sine wave
dataToSend1 = string(amplitude1 * sin(2 * pi * frequency1 * t(it)));
dataToSend2 = string(amplitude2 * sin(2 * pi * frequency2 * t(it)));

dataStructArray(1) = dataToSend1;
dataStructArray(2) = dataToSend2;
dataStructArray(3) = dataToSend1;
dataStructArray(4) = dataToSend2;
dataStructArray(5) = dataToSend1;
dataStructArray(6) = dataToSend2;
dataStructArray(7) = dataToSend1;
dataStructArray(8) = dataToSend2;

% Convert the struct array to a JSON-formatted string
jsonString = jsonencode(dataStructArray);
disp(jsonString)

% Send data
fwrite(udpObject, jsonString, 'uint8');

end

% Close the UDP connection
fclose(udpObject);
delete(udpObject);
clear udpObject;
