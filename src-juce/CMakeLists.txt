# DarkMark (C) 2019-2021 Stephane Charette <stephanecharette@gmail.com>


IF (WIN32)
	SET (RT "")
	SET (DL "")
	SET (X11 "")
ELSE ()
	FIND_LIBRARY	( RT		rt			)
	FIND_LIBRARY	( DL		dl			)
	FIND_PACKAGE	( X11		REQUIRED	)
ENDIF ()
FIND_PACKAGE	( Threads	REQUIRED	)
FIND_PACKAGE	( Freetype	REQUIRED	)
INCLUDE_DIRECTORIES ( AFTER ${FREETYPE_INCLUDE_DIRS} )
SET ( JUCE_LIBRARIES ${RT} ${DL} ${X11_LIBRARIES} ${FREETYPE_LIBRARIES} ${GTK3_LIBRARIES} )

ADD_DEFINITIONS ( -DJUCE_USE_CURL=0 )
ADD_DEFINITIONS ( -DJUCE_WEB_BROWSER=0 )
ADD_DEFINITIONS ( -DJUCE_ENABLE_LIVE_CONSTANT_EDITOR=0 )
ADD_DEFINITIONS ( -DJUCE_STANDALONE_APPLICATION=1 )
ADD_DEFINITIONS ( -DJUCE_REPORT_APP_USAGE=1 )
ADD_DEFINITIONS ( -DJUCE_DISPLAY_SPLASH_SCREEN=1 )
ADD_DEFINITIONS ( -DJUCE_USE_DARK_SPLASH_SCREEN=0 )
ADD_DEFINITIONS ( -DJUCE_CATCH_UNHANDLED_EXCEPTIONS=1 )

# create a list of all the JUCE source files we need to compile
FILE ( GLOB JUCE_SOURCE ${CMAKE_HOME_DIRECTORY}/JuceLibraryCode/*.cpp )
LIST ( SORT JUCE_SOURCE )

ADD_LIBRARY ( dm_juce STATIC ${JUCE_SOURCE} )
TARGET_LINK_LIBRARIES ( dm_juce ${CMAKE_THREAD_LIBS_INIT} ${JUCE_LIBRARIES} )
