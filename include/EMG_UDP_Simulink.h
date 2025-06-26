#ifndef EMG_UDP_SIMULINK_H_
#define EMG_UDP_SIMULINK_H_

#define WIN32_LEAN_AND_MEAN
// No need for Windows-specific socket headers

#include <string>
#include <vector> // Required for std::vector<double> maxAmp_

// --- Unix Socket Includes ---
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "execution.hxx"
#include "ExecutionEmgXml.h" // Assumed to expose getMaxEmg() and setMaxEmg()
#include "ProducersPluginVirtual.h"
#include "NMSmodel.hxx"
#include <Filter.h>
#include <chrono>
#include <ctime>
#include "OpenSimFileLogger.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <getTime.h>
#include <memory>

#ifdef WIN32
class __declspec(dllexport) EMGUDPSimulink : public ProducersPluginVirtual
#endif
#if UNIX
class EMGUDPSimulink : public ProducersPluginVirtual
#endif

{
public:
	/**
	* Constructor
	*/
	EMGUDPSimulink();

	/**
	* Destructor
	*/
	virtual ~EMGUDPSimulink();

	/**
	* Initialization method
	* @param xmlName Subject specific XML
	* @param executionName execution XML for CEINMS-RT software configuration
	*/
	void init(std::string xmlName, std::string executionName);

	void reset()
	{
	}

	/**
	* Get the data with the name of the EMG channel mapping the EMG.
	* The correspondence between the channel and the muscle are in the subject specific XML.
	*/
	const std::map<std::string, double>& GetDataMap();

	/**
	* Get a set of the channel name
	*/
	const std::set<std::string>& GetNameSet()
	{
		return nameSet_;
	}

	/**
	* Get the time stamp of the EMG capture.
	*/
	const double& getTime()
	{
		timeInitMutex_.lock();
		timeSafe_ = timenow_;
		timeInitMutex_.unlock();
		return timeSafe_;
	}

	void stop();

	void setDirectories(std::string outDirectory, std::string inDirectory = std::string())
	{
		_outDirectory = outDirectory;
		_inDirectory = inDirectory;
	}

	void setVerbose(int verbose)
	{
	}

	void setRecord(bool record)
	{
		_record = record;
	}

	const std::map<std::string, double>& GetDataMapTorque()
	{
		return _torque;
	}
protected:

	void EMGFeed();
	void testConnect()
	{
		if (_connect == false)
		{
			std::cout << "Not connected to Delsys server !" << std::endl << std::flush;
			exit(EXIT_FAILURE);
		}
	}


	std::map<std::string, double> mapData_; //!< Map between the name of the channel and the EMG
	std::set<std::string> nameSet_; //!< Set of the name of the channel
	std::vector<std::string> nameVect_; //!< Vector of channel names
	unsigned short emgCpt_; //!< index in the data packet being read in the GetDataMap() method
	double time_; //!< Time stamp of the EMG
	double timeSafe_; //!< Thread safe time stamp for getTime() method
	double timeInit_;

	std::shared_ptr<std::thread> feederThread; //!< Thread for the filtering of the data


	std::mutex DataMutex_; //!< Mutex for the data
	std::mutex EMGMutex_; //!< Mutex for the Raw EMG data
	std::mutex timeInitMutex_; //!< Mutex for the Raw EMG data
	std::mutex loggerMutex_; //!< Mutex for the Raw EMG data

	std::map<std::string, double> _torque;

	bool threadEnd_;
	bool newData_;

	std::string _outDirectory;
	std::string _inDirectory;

	bool _record;
	bool _connect;

	std::vector<double> dataEMG_;
	double timenow_;

	std::string ip_;
	int port_;

	OpenSimFileLogger<int>* _logger;

	ExecutionEmgXml* _executionEmgXml;

	int emgSockFd;		//EMG socket file descriptor

    // --- NEW: For maxAmp calibration and normalization ---
    std::vector<double> maxAmp_;            // Stores the maximum amplitude for each EMG channel
    // calibrationMode_ and enableNormalization_ are now implicitly handled
};

#endif