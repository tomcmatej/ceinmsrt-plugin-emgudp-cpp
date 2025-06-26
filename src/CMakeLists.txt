ADD_LIBRARY( EMG_UDP_Simulink SHARED EMG_UDP_Simulink.cpp
)


TARGET_LINK_LIBRARIES(EMG_UDP_Simulink

	XMLEMGInterpreter
	XMLExecutionInterpreter
	FileIO
	FileLogger
	${Boost_LIBRARIES}
	XercesC::XercesC
	pthread
)


#ADD_EXECUTABLE(testPlugin test.cpp
#)

#TARGET_LINK_LIBRARIES(
#	EMG_UDP_Simulink
#)

#ADD_EXECUTABLE(testServerEMG testServerEMG.cpp
#)

# ADD_EXECUTABLE(UDPServer UDPServer.cpp
# )

# ADD_EXECUTABLE(UDPClient UDPClient.cpp
# )

# TARGET_LINK_LIBRARIES(UDPServer
# 	${WINLIB}
# 	${WINMM}
# )

# TARGET_LINK_LIBRARIES(UDPClient
# 	${WINLIB}
# 	${WINMM}
# )