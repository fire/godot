
#include "GL/gl3w.h"

#include <stdio.h>
#include <stdlib.h>

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();

	extern bool g_hasDebugObjectLabel;

	static void APIENTRY GLDebugCallback( GLenum source, GLenum type, GLuint id, GLenum severity,
										  GLsizei length, const GLchar *message, const void *userParam )
	{
		printf( "%s\n", message );
	}

	void initBetsyPlatform()
	{

		int width = 1280;
		int height = 720;

		int screen = 0;
		
		// GL 4.3 mandates GL_KHR_debug
		g_hasDebugObjectLabel = true;

		const bool gl3wFailed = gl3wInit() != 0;
		if( gl3wFailed )
		{
			fprintf( stderr, "Failed to initialize GL3W\n" );
			abort();
		}

#ifdef DEBUG
		glDebugMessageCallback( &GLDebugCallback, NULL );
		glEnable( GL_DEBUG_OUTPUT );
		glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
#endif
	}

	void shutdownBetsyPlatform()
	{
	}

	void pollPlatformWindow()
	{
	}
}  // namespace betsy
