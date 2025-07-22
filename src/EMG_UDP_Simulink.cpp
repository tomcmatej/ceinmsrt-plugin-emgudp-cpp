#include "EMG_UDP_Simulink.h" // Corrected to use the proper header filename

// Standard C++ Library Includes
#include <mutex>
#include <thread>
#include <cmath>
#include <iostream> // For std::cout, std::cerr
#include <map>
#include <algorithm> // For std::fill, std::replace
#include <stdexcept> // For std::runtime_error
#include <string>    // For std::string
#include <vector>    // For std::vector
#include <fstream>   // For std::ifstream
#include <cstdio>    // For remove()
#include <sstream>   // NEW: For std::istringstream

// Unix-specific includes
#include <errno.h>   // For errno and strerror
#include <cstring>   // For strtok (not used in new parsing), memset, strcpy


// CEINMS-RT specific includes
#include <getTime.h>         // Custom time utility
#include <ExecutionXmlReader.h> // For reading CEINMS configuration



// Constructor initializes members
EMGUDPSimulink::EMGUDPSimulink() : emgSockFd(-1), _logger(nullptr), _executionEmgXml(nullptr)
{
	_record = false;
	_connect = false;
    threadEnd_ = false; // Initialize to false
    newData_ = false;   // Initialize to false
    timenow_ = 0.0;     // Initialize time
    
    // maxAmp_ will be initialized in init()
}

// Destructor ensures proper cleanup
EMGUDPSimulink::~EMGUDPSimulink()
{
    // Call stop() to ensure the thread is joined and socket is closed gracefully
    stop();

    // Clean up dynamically allocated memory
    if (_executionEmgXml) {
        delete _executionEmgXml;
        _executionEmgXml = nullptr;
    }
}

void EMGUDPSimulink::init(std::string xmlName, std::string executionName)
{
	// Default IP and Port (will be overwritten by XML configuration)
	ip_ = "127.0.0.1";
	port_ = 31000;

	// Initialize logger if recording is enabled
	if (_record) {
		if (_outDirectory.empty()) {
            throw std::runtime_error("Output directory not set for logging in EMG plugin. It is critical for logging.");
        }
		_logger = new OpenSimFileLogger<int>(_outDirectory);
	}

	// Parse Subject XML
	std::unique_ptr<NMSmodelType> subjectPointer; // Using unique_ptr for modern C++ practice
	try
	{
		subjectPointer = std::unique_ptr<NMSmodelType>(::subject(xmlName, xml_schema::flags::dont_initialize));
	}
	catch (const xml_schema::exception& e)
	{
		std::cerr << "Error parsing Subject XML ('" << xmlName << "'): " << e << std::endl;
		exit(EXIT_FAILURE);
	}

	NMSmodelType::Channels_type& channels(subjectPointer->Channels());
	ChannelsType::Channel_sequence& channelSequence(channels.Channel());

	std::cout << "EMG_UDP_Simulink: Reading Execution XML from: " << executionName << std::endl;

	// Parse Execution XML
	std::unique_ptr<ExecutionType> executionPointer; // Using unique_ptr
	try
	{
		executionPointer = std::unique_ptr<ExecutionType>(::execution(executionName, xml_schema::flags::dont_initialize));
	}
	catch (const xml_schema::exception& e)
	{
		std::cerr << "Error parsing Execution XML ('" << executionName << "'): " << e << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string EMGFile = executionPointer->ConsumerPlugin().EMGDeviceFile().get().c_str();

	_executionEmgXml = new ExecutionEmgXml(EMGFile); // Manually managed, deleted in destructor

	ip_ = _executionEmgXml->getIP();
	port_ = atoi(_executionEmgXml->getPort().c_str());

    // --- NEW: Read initial maxAmp_ values from ExecutionEmgXml ---
    // Assuming ExecutionEmgXml has a public const std::vector<double>& getMaxEmg() const method
    try {
        maxAmp_ = _executionEmgXml->getMaxEmg(); // Using getMaxEmg() from ExecutionEmgXml
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not read initial maxAmp values from executionEMG.xml. " << e.what() << ". Initializing to defaults." << std::endl;
        // Will be resized and initialized below.
    }

	// Get EMG channel names from subject XML
	for (ChannelsType::Channel_iterator it = channelSequence.begin(); it != channelSequence.end(); ++it)
	{
		nameSet_.insert(it->name());
		nameVect_.push_back(it->name());
	}

    // --- NEW: Initialize/Sanitize maxAmp_ based on channel count ---
    if (nameVect_.empty()) {
        throw std::runtime_error("No EMG channels defined in subject XML. Cannot proceed.");
    }
    if (maxAmp_.size() != nameVect_.size()) {
        std::cerr << "Warning: Loaded maxAmp_ size (" << maxAmp_.size() << ") mismatch with number of channels (" << nameVect_.size() << "). Resizing." << std::endl;
        maxAmp_.resize(nameVect_.size());
    }

    // Initialize maxAmp_ values to 1.0 if they are 0 or problematic.
    // This ensures no division by zero during normalization.
    // If maxEMG in XML is 0.0, this first run will effectively be the calibration run
    // where maxAmp_ will accumulate real maximums.
    for(size_t i = 0; i < maxAmp_.size(); ++i) {
        if (maxAmp_[i] < 1.0e-6) { // Check against a small epsilon instead of exactly 0
            maxAmp_[i] = 1.0; // Default to 1.0 if value is problematic (allows normalization to proceed)
        }
    }
    std::cout << "EMG_UDP_Simulink: Plugin initialized. MaxEMG values loaded/defaulted. Normalization will apply." << std::endl;


	// Add log for EMG if recording
	if (_record)
	{
		_logger->addLog(Logger::EmgsFilter, nameVect_);
	}

	threadEnd_ = true; // Set flag to allow thread to run
    newData_ = false;   // No new data yet

    // --- UDP Socket Setup (Unix specific) ---
    emgSockFd = socket(AF_INET, SOCK_DGRAM, 0); // Create IPv4 UDP socket
    if (emgSockFd < 0) {
        throw std::runtime_error("Failed to create EMG UDP socket: " + std::string(strerror(errno)));
    }
    std::cout << "EMG_UDP_Simulink: UDP Socket created with FD: " << emgSockFd << std::endl;

    // Allow reuse of address/port (useful for quick restarts during development)
    int reuse = 1;
    if (setsockopt(emgSockFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed for EMG socket: " << strerror(errno) << std::endl;
    }
    if (setsockopt(emgSockFd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "Warning: setsockopt(SO_REUSEPORT) failed for EMG socket: " << strerror(errno) << std::endl;
    }

    struct sockaddr_in serverInfo;
    memset(&serverInfo, 0, sizeof(serverInfo)); // Clear the structure
    serverInfo.sin_family = AF_INET;           // IPv4
    serverInfo.sin_port = htons(port_);        // Port to listen on (network byte order)
    
    // Convert IP string to binary form (for specific IP binding, e.g., "127.0.0.1")
    if (inet_pton(AF_INET, ip_.c_str(), &(serverInfo.sin_addr)) <= 0) {
        close(emgSockFd);
        throw std::runtime_error("Invalid IP address for EMG socket: " + ip_);
    }

    // Bind the socket to the specified IP and port
    if (::bind(emgSockFd, (const struct sockaddr*)&serverInfo, sizeof(serverInfo)) < 0) {
        close(emgSockFd);
        throw std::runtime_error("Failed to bind EMG UDP socket to " + ip_ + ":" + std::to_string(port_) + ": " + strerror(errno));
    }
    std::cout << "EMG_UDP_Simulink: UDP Socket bound to " << ip_ << ":" << port_ << std::endl;

    // Start the background thread for EMG data reception and processing
	feederThread = std::make_shared<std::thread>(&EMGUDPSimulink::EMGFeed, this);
}

void EMGUDPSimulink::stop()
{
	threadEnd_ = false; // Signal the communication thread to stop

    // Ensure the thread exists and is joinable before trying to join it
    if (feederThread && feederThread->joinable()) {
        feederThread->join(); // Wait for the thread to complete its execution
    }

    // --- NEW: Save final maxAmp_ values to XML and print to console ---
    // This is done automatically as the plugin continuously updates maxAmp_.
    if (_executionEmgXml) {
        try {
            // This calls ExecutionEmgXml::setMaxEmg() which is assumed to exist (as per PluginEMGROS.cpp)
            _executionEmgXml->setMaxEmg(maxAmp_); 
            _executionEmgXml->UpdateEmgXmlFile(); // Write to the original executionEMG.xml filename
            std::cout << "EMG_UDP_Simulink: MaxEMG values saved to executionEMG xml" << std::endl;
            
            // Print maxAmp_ to console for user to copy/paste if needed
            std::cout << "Final maxEMG values (space-separated): ";
            for (double val : maxAmp_) {
                std::cout << val << " ";
            }
            std::cout << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "ERROR: Failed to write maxEMG values to XML: " << e.what() << std::endl;
        }
    }

    // Clean up logger if it was created and recording
	if (_record && _logger)
	{
		_logger->stop();
		delete _logger;
		_logger = nullptr; // Set to nullptr to avoid dangling pointer
	}
    
    // Close the socket file descriptor if it's open
    if (emgSockFd != -1) {
        close(emgSockFd); // Unix-specific socket close
        emgSockFd = -1; // Invalidate the file descriptor
    }
}

void EMGUDPSimulink::EMGFeed()
{
	struct sockaddr_in clientAddr; // Struct to store the sender's address
	socklen_t clientAddrSize = sizeof(clientAddr); // Size of the sender's address struct
	char emgUDPBuffer[1024]; // Buffer for received UDP data
	int bytesRead;	
	const int NBOFCHANNEL = nameVect_.size(); // Number of EMG channels expected
	double timeInitCpy; // Local variable for timestamp

    // Set a receive timeout for the socket to prevent indefinite blocking
    // This allows the thread to check `threadEnd_` regularly and exit cleanly.
    struct timeval tv;
    tv.tv_sec = 0;           // 0 seconds
    tv.tv_usec = 100000;     // 100 milliseconds
    if (setsockopt(emgSockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "Warning: setsockopt(SO_RCVTIMEO) failed for EMG socket: " << strerror(errno) << std::endl;
    }

    int receiveCnt = 0; // Counter for received packets DEBUG

	while (threadEnd_) { // Loop as long as the `threadEnd_` flag is true
		std::vector<double> tempEMGdata;
		tempEMGdata.resize(NBOFCHANNEL); // Resize vector to hold expected number of channels

		// Get current time (thread-safe access)
		timeInitMutex_.lock();
		timenow_ = rtb::getTime();
		timeInitCpy = timenow_;
		timeInitMutex_.unlock();

		// Receive data from the UDP socket
        // -1 from buffer size for null terminator
		bytesRead = recvfrom(emgSockFd, emgUDPBuffer, sizeof(emgUDPBuffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrSize);

		if (bytesRead < 0) {
            // Check if error is due to timeout (no data received within time)
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; // No data, just continue to next loop iteration
            }
            // If the thread is signaling to stop, and it's not a timeout error, break loop
            if (!threadEnd_) {
                break;
            }
            // Report other (unexpected) receive errors

			std::cerr << "EMG Receive Failed: " << strerror(errno) << std::endl;
			break; // Fatal error, break the loop
		}
        if (bytesRead == 0) { // Should not happen for UDP usually, but defensive check
            continue;
        }

		emgUDPBuffer[bytesRead] = '\0'; // Null-terminate the received string for C string functions
		
        
        receiveCnt++;
        if (receiveCnt>=1000) {
            std::cout << "EMG_UDP_Simulink: Received " << bytesRead << " bytes. Content: '" << emgUDPBuffer << "'" << std::endl; // MODIFIED THIS LINE     
            receiveCnt=0;
        }


        // --- NEW: Robust Parsing (replacing strtok) ---
        std::string received_str(emgUDPBuffer);
        
        // Find and remove leading '[' and trailing ']'
        size_t first_bracket = received_str.find('[');
        size_t last_bracket = received_str.rfind(']');
        if (first_bracket == std::string::npos || last_bracket == std::string::npos || last_bracket <= first_bracket) {
            std::cerr << "ERROR: Received string missing brackets or malformed: " << received_str << std::endl;
            std::fill(tempEMGdata.begin(), tempEMGdata.end(), 0.0); // Fill with zeros on error
            continue; // Skip processing this bad packet
        }
        std::string inner_content = received_str.substr(first_bracket + 1, last_bracket - first_bracket - 1);

        // Replace all " and , with spaces to simplify tokenization by stringstream
        std::replace(inner_content.begin(), inner_content.end(), '"', ' ');
        std::replace(inner_content.begin(), inner_content.end(), ',', ' ');
        
        std::istringstream iss(inner_content);
        double value;
        int iLoc = 0;
        while (iss >> value && iLoc < NBOFCHANNEL) { // Extract doubles until end of stream or all channels filled
            tempEMGdata[iLoc] = value;
            iLoc++;
        }
        // Fill any remaining channels with 0.0 if not enough values were parsed
        while (iLoc < NBOFCHANNEL) {
            tempEMGdata[iLoc] = 0.0;
            iLoc++;
        }

        // --- NEW: Continuous maxAmp Accumulation ---
        // Max values are always tracked, regardless of calibration "mode".
        // This is the core of the running maximum.
        for (size_t i = 0; i < NBOFCHANNEL; ++i) {
            // Ensure tempEMGdata[i] is non-negative before comparison if EMG is usually positive.
            // If raw EMG can be negative (e.g., from AC-coupling/filtering), consider abs(tempEMGdata[i]).
            // Assuming EMG values should be positive after rectification/processing for maxAmp.
            if (tempEMGdata[i] > maxAmp_[i]) {
                maxAmp_[i] = tempEMGdata[i];
            }
        }

        // --- NEW: Normalization Logic (Always apply if maxAmp_ is valid) ---
        // Normalization is always applied using the current maxAmp_.
        // If maxAmp_ for a channel is still 1.0 (from default init), normalization has no effect.
        for (int iLoc = 0; iLoc < NBOFCHANNEL; ++iLoc) {
            // Apply normalization using the *currently tracked* maxAmp_
            if (maxAmp_[iLoc] > 1.0e-6) { // Avoid division by very small numbers or zero
                tempEMGdata[iLoc] = tempEMGdata[iLoc] / maxAmp_[iLoc];
                // Cap the normalized value at 1.0, similar to "NoMax" variant logic
                if (tempEMGdata[iLoc] > 1.0) {
                    tempEMGdata[iLoc] = 1.0;
                }
            } else {
                // If maxAmp_ is problematic (0 or very small), treat normalized EMG as 0
                // This means data won't be normalized (or will be 0) until maxAmp_ for that channel becomes > 0
                tempEMGdata[iLoc] = 0.0;
            }
        }


		EMGMutex_.lock();
		dataEMG_ = tempEMGdata; // Copy processed (accumulated max / normalized) data to shared member
		newData_ = true;        // Signal that new data is available
		EMGMutex_.unlock();

		// Log data if recording is enabled
		if (_record)
		{
			loggerMutex_.lock();
			_logger->log(Logger::EmgsFilter, timeInitCpy, tempEMGdata);
			loggerMutex_.unlock();
		}
	}
    std::cout << "EMG_UDP_Simulink: UDP Communication thread stopped." << std::endl;
}

const std::map<std::string, double>& EMGUDPSimulink::GetDataMap()
{
	// Local vector to safely copy data
	std::vector<double> dataEMGSafe;
	DataMutex_.lock(); // Lock mutex before accessing shared data

	if (newData_) // Check if there's new data available
	{
		dataEMGSafe = dataEMG_; // Copy data
		newData_ = false;       // Reset flag, data has been consumed
	}
	else
	{
		DataMutex_.unlock(); // Unlock before returning if no new data
		// If no new data, fill map with zeros
		for (const auto& name : nameVect_) // C++11 range-based for loop
		{
			mapData_[name] = 0.0;
		}
		return mapData_;
	}
	DataMutex_.unlock(); // Unlock mutex after accessing shared data

    // Check for data size consistency after copying
	if (dataEMGSafe.empty() || dataEMGSafe.size() < nameVect_.size())
	{
        std::cerr << "Warning: EMG data size mismatch or empty after copy. Received: " << dataEMGSafe.size()
                  << ", Expected: " << nameVect_.size() << ". Filling with zeros." << std::endl;
		for (const auto& name : nameVect_)
		{
			mapData_[name] = 0.0;
		}
		return mapData_;
	}

	// Fill the map with received data
	for (size_t idx = 0; idx < nameVect_.size(); ++idx)
	{
		mapData_[nameVect_[idx]] = dataEMGSafe[idx];
	}
	return mapData_;
}


// Factory functions (remain unchanged for CEINMS to load the plugin)
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
#ifdef WIN32 // __declspec (dllexport) is important for dynamic loading
extern "C" __declspec (dllexport) ProducersPluginVirtual * __cdecl create() {
	return new EMGUDPSimulink;
}

extern "C" __declspec (dllexport) void __cdecl destroy(ProducersPluginVirtual * p) {
	delete p;
}
#endif