#include "EMG_UDP_Simulink.h"

EMGUDPSimulink::EMGUDPSimulink()
{
	_record = false;
	_connect = false;
}

EMGUDPSimulink::~EMGUDPSimulink()
{
	delete _executionEmgXml;
	delete feederThread;
}

void EMGUDPSimulink::init(std::string xmlName, std::string executionName)
{
	// Default port and IP
	ip_ = "127.0.0.1";
	port_ = 31000;

	//If we save the data create logger class
	if (_record)
		_logger = new OpenSimFileLogger<int>(_outDirectory);

	//pointer to subject XML
	std::auto_ptr<NMSmodelType> subjectPointer;

	try
	{
		subjectPointer = std::auto_ptr<NMSmodelType>(subject(xmlName, xml_schema::flags::dont_initialize));
	}
	catch (const xml_schema::exception& e)
	{
		cout << e << endl;
		exit(EXIT_FAILURE);
	}

	NMSmodelType::Channels_type& channels(subjectPointer->Channels());
	ChannelsType::Channel_sequence& channelSequence(channels.Channel());

	std::cout << "Reading Execution XML from: " << executionName << std::endl;

	// Configuration XML
	std::auto_ptr<ExecutionType> executionPointer;

	try
	{
		std::auto_ptr<ExecutionType> temp(execution(executionName, xml_schema::flags::dont_initialize));
		executionPointer = temp;
	}
	catch (const xml_schema::exception& e)
	{
		cout << e << endl;
		exit(EXIT_FAILURE);
	}

	// Get the EMG XML configuration
	const std::string& EMGFile = executionPointer->ConsumerPlugin().EMGDeviceFile().get();

	// EMG XML class reader
	_executionEmgXml = new ExecutionEmgXml(EMGFile);

	//Ip and Port from the EMG xml
	ip_ = _executionEmgXml->getIP();
	port_ = atoi(_executionEmgXml->getPort().c_str());

	//Get the EMG channel from the subject xml
	for (ChannelsType::Channel_iterator it = channelSequence.begin(); it != channelSequence.end(); it++)
	{
		//get name channel
		nameSet_.insert(it->name());
		nameVect_.push_back(it->name());
	}

	//Create file for saving data
	if (_record)
	{
		_logger->addLog(Logger::EmgsFilter, nameVect_);
	}

	//Bool to stop thread
	threadEnd_ = true;

	// thread for the filtering of the data
	feederThread = new std::thread(&EMGUDPSimulink::EMGFeed, this);
}

void EMGUDPSimulink::stop()
{
	threadEnd_ = false;
	feederThread->join();

	if (_record)
	{
		_logger->stop();
		delete _logger;
	}
}

void EMGUDPSimulink::EMGFeed()
{
	SOCKET socketC;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	sockaddr_in serverInfo;
	int len = sizeof(serverInfo);
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_port = htons(port_);
	serverInfo.sin_addr.s_addr = inet_addr(ip_.c_str());

	socketC = socket(AF_INET, SOCK_DGRAM, 0);

	if (socketC == INVALID_SOCKET) {
		std::cerr << "Failed to create socket" << std::endl;
		WSACleanup();
	}

	if (::bind(socketC, (sockaddr*)&serverInfo, sizeof(serverInfo)) == SOCKET_ERROR) {
		std::cerr << "Bind Failed" << std::endl;
		closesocket(socketC);
		WSACleanup();
	}

	char emgUDPBuffer[1024];
	sockaddr_in clientAddr;
	int clientAddrSize = sizeof(clientAddr);
	int bytesRead;	
	const int NBOFCHANNEL = nameVect_.size();
	double timeInitCpy;

	while (threadEnd_) {
		std::vector<double> tempEMGdata;
		tempEMGdata.resize(NBOFCHANNEL);

		timeInitMutex_.lock();
		timenow_ = rtb::getTime();
		timeInitCpy = timenow_;
		timeInitMutex_.unlock();

		bytesRead = recvfrom(socketC, emgUDPBuffer, sizeof(emgUDPBuffer), 0, (sockaddr*)&clientAddr, &clientAddrSize);
		if (bytesRead == SOCKET_ERROR) {
			std::cerr << "Receive Failed" << std::endl;
			break;
		}

		emgUDPBuffer[bytesRead] = '\0';
		std::cout << "Received data from " << inet_ntoa(clientAddr.sin_addr) << ":" << emgUDPBuffer << std::endl;


		char* token  = strtok(const_cast<char*>(emgUDPBuffer), "\"[");

		for (int iLoc = 0; iLoc < tempEMGdata.size();++iLoc) {
			double value = std::strtod(token, nullptr);
			tempEMGdata[iLoc]=value;
			token = strtok(nullptr, "\",");
		}


		EMGMutex_.lock();
		dataEMG_ = tempEMGdata;
		newData_ = true;
		EMGMutex_.unlock();
		if (_record)
		{
			loggerMutex_.lock();
			_logger->log(Logger::EmgsFilter, timeInitCpy, tempEMGdata);
			loggerMutex_.unlock();
		}
		
		
	}

	closesocket(socketC);
	WSACleanup();
}


const std::map<std::string, double>& EMGUDPSimulink::GetDataMap()
{

	// Get the data
	std::vector<double> dataEMGSafe;
	DataMutex_.lock();

	//if we do not have data send 0
	if (newData_)
	{
		dataEMGSafe = dataEMG_;
	}
	else
	{
		DataMutex_.unlock();
		for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
		{
			mapData_[*it] = 0;
		}
		return mapData_;
	}
	DataMutex_.unlock();

	if (dataEMGSafe.size() < nameVect_.size())
	{
		for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
		{
			mapData_[*it] = 0;
		}
		return mapData_;
	}

	// Fill the map
	for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
	{
		mapData_[*it] = dataEMGSafe[std::distance<std::vector<std::string>::const_iterator>(nameVect_.begin(), it)];
	}
	return mapData_;
}


#ifdef UNIX
extern "C" ProducersPluginVirtual * create()
{
	return new EMGUDPSimulink;
}

extern "C" void destroy(ProducersPluginVirtual * p)
{
	delete p;
}
#endif
#ifdef WIN32 // __declspec (dllexport) id important for dynamic loading
extern "C" __declspec (dllexport) ProducersPluginVirtual * __cdecl create() {
	return new EMGUDPSimulink;
}

extern "C" __declspec (dllexport) void __cdecl destroy(ProducersPluginVirtual * p) {
	delete p;
}
#endif