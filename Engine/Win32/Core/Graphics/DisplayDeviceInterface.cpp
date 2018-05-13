#include "Graphics/DisplayDeviceInterface.h"
#include "Debug/NeptuneDebug.h"
#include "Debug/StandardErrorCodes.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>

using namespace Neptune;

const  u32 FRAME_BUFFER_OBJECT_UNDEFINED = ~0;
static u32 s_frame_buffer_object_index = 1;// FRAME_BUFFER_OBJECT_UNDEFINED; // must be renamed
static u32 s_frame_buffer_height = 0;
static u32 s_frame_buffer_width = 0;
static u32 s_window_height = 0;
static u32 s_window_width = 0;
static float s_clear_depth_value = 1.0f; // without reversed-z, the right value is 1.0f

// Returns a value equal to the number of times a pixel can be sampled. Returns NEP_STD_U8_ERROR_CODE_0 if the input value is not supported.
static u8 MapMultiSampleAntiAlliasingValues(DisplayDeviceInterface::MULTI_SAMPLE_ANTI_ALLIASING _v)
{
	using namespace DisplayDeviceInterface;

	switch (_v)
	{
	case MULTI_SAMPLE_ANTI_ALLIASING::NONE:
		return 0;

	case MULTI_SAMPLE_ANTI_ALLIASING::X2:
		return 2;

	case MULTI_SAMPLE_ANTI_ALLIASING::X4:
		return 4;

	case MULTI_SAMPLE_ANTI_ALLIASING::X8:
		return 8;

	case MULTI_SAMPLE_ANTI_ALLIASING::X16:
		return 16;

	default:
		return NEP_STD_U8_ERROR_CODE_0; // Error unsupported
	}
}

static bool DidUserTryToEnableOffScreenRendering(const DisplayDeviceInterface::GraphicalContextSettings& _userSettings)
{
	return _userSettings.m_frameBufferHeight > 0 && _userSettings.m_frameBufferWidth > 0;
}

static bool CreateFBOAndEnableReversedZIfNeeded(const DisplayDeviceInterface::GraphicalContextSettings& _userSettings)
{
	NEP_GRAPHICS_ASSERT();
	
	// Set frame-buffer's dimension
	s_frame_buffer_height = _userSettings.m_frameBufferHeight;
	s_frame_buffer_width  = _userSettings.m_frameBufferWidth;
	
	// Modify clipping policy if reversed-z is wanted
	if (_userSettings.m_enableReversedZ)
	{
		//if ((major > 4 || (major == 4 && minor >= 5)) ||
		if (!SDL_GL_ExtensionSupported("GL_ARB_clip_control"))
		{
			NEP_LOG("Error, Reversed-z is not supported.");
		}

		// Override OpenGL default clipping policy -
		// clipping is done between [-1.0, 1.0], here it is
		// forced to [0.0, 1.0]
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		NEP_GRAPHICS_ASSERT();
	}
	
	// Generate the colour buffer associated to the frame buffer
	u32 color = 0, depth = 0, fbo = 0;
	u32 texture_type = (_userSettings.m_antiAliasing != DisplayDeviceInterface::MULTI_SAMPLE_ANTI_ALLIASING::NONE) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	u8 multi_sampling_factor = MapMultiSampleAntiAlliasingValues(_userSettings.m_antiAliasing);

	// multisampled colour bufer
	glGenTextures(1, &color);
	glBindTexture(texture_type, color);

	if (_userSettings.m_antiAliasing != DisplayDeviceInterface::MULTI_SAMPLE_ANTI_ALLIASING::NONE){
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multi_sampling_factor, GL_RGBA8, _userSettings.m_frameBufferWidth, _userSettings.m_frameBufferHeight, false);
	}
	else{
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _userSettings.m_frameBufferWidth, _userSettings.m_frameBufferHeight);
	}
	glBindTexture(texture_type, 0);
	NEP_GRAPHICS_ASSERT(); 

	// multi-sampled depth-buffer
	u32 depth_binary_format = (_userSettings.m_enableReversedZ) ? GL_DEPTH_COMPONENT32F : GL_DEPTH_COMPONENT32_ARB; // 32-bit float if reversed-z is enabled, 32-bit int otherwise.

	glGenTextures(1, &depth);
	NEP_GRAPHICS_ASSERT();

	glBindTexture(texture_type, depth);
	NEP_GRAPHICS_ASSERT();

	if (_userSettings.m_antiAliasing != DisplayDeviceInterface::MULTI_SAMPLE_ANTI_ALLIASING::NONE){
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multi_sampling_factor, depth_binary_format, _userSettings.m_frameBufferWidth, _userSettings.m_frameBufferHeight, false);
	}
	else {
		glTexStorage2D(GL_TEXTURE_2D, 1, depth_binary_format, _userSettings.m_frameBufferWidth, _userSettings.m_frameBufferHeight);
	}
	NEP_GRAPHICS_ASSERT();

	glBindTexture(texture_type, 0);
	NEP_GRAPHICS_ASSERT();

	// bind frame buffer
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_type, color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_type, depth, 0);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status != GL_FRAMEBUFFER_COMPLETE) {
		NEP_LOG("glCheckFramebufferStatus: %x\n", status);
	}

	// Bind the frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	NEP_GRAPHICS_ASSERT();

	return true;
}

// Must be reimplemented on each platform that uses SDL
static bool InitContext()
{
	glewExperimental = GL_TRUE;
	if(glewInit() != GLEW_OK)
	{
		NEP_LOG("Error: Graphical Context couldn't be initialized");
		SDL_Quit();
		return false;
	}

	return true;
}

DisplayDeviceInterface::GraphicalContextSettings::GraphicalContextSettings():
	m_antiAliasing(MULTI_SAMPLE_ANTI_ALLIASING::NONE),
	m_frameBufferHeight(0),
	m_frameBufferWidth(0),
	m_enableReversedZ(false)
{

}

DisplayDeviceInterface::WindowHandle DisplayDeviceInterface::CreateWindow(const char* _name, u32 _width, u32 _height, MULTI_SAMPLE_ANTI_ALLIASING _antiAliasing /*= MULTI_SAMPLE_ANTI_ALLIASING::NONE*/, bool _fullScreen /*= false*/)
{
	// Default to OpenGL 4.3
	const u8 OGL_MAJOR_VERSION   = 4;
	const u8 OGL_MINOR_VERSION   = 3;

	s_window_height = _height;
	s_window_width  = _width;

	// The compatibility profile must be used on Windows because of a bug in Glew
	// that causes invalid GL_ENUM_ERRORS to be raised.
	const u8 OGL_CONTEXT_PROFILE = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
	
	// Initialize the SDL's video system
	if(SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		NEP_LOG("ERROR: Couldn't Initialize video system\n");
		return nullptr;
	}

	// Prior window-creation context-initialization
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,  OGL_CONTEXT_PROFILE );	// Selects the OpenGl profile to use
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OGL_MAJOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, OGL_MINOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);							// Enable hardware acceleration
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);								// Enable double buffering

	// Enable multi-sampled anti aliasing
	if (_antiAliasing != MULTI_SAMPLE_ANTI_ALLIASING::NONE)
	{
		u8 multisample = MapMultiSampleAntiAlliasingValues(_antiAliasing);
		if (multisample == NEP_STD_U8_ERROR_CODE_0)
		{
			multisample = 0; // fallback to no anti-alliasing support
			NEP_LOG("Warning DisplayDeviceInterface::CreateWindow : Multi-sampling is not supported. Falling back to not anti-aliased mode.");
		}

		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);				// Enables multi-sampling anti-alliasing
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample);
	}

	// Depth test is enabled by default with a depth of 16 bits. The following line is present for readability purpose.
	// This value is conserved because even on recent Intel architectures, 32-bit depth buffers are not supported.
	// Furthermore, SDL_GL_SetAttribute doesn't return any errors if the size provided in input is too big.
	const s32 DEFAULT_DEPTH_SIZE = 16;
	//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, DEFAULT_DEPTH_SIZE);

	// Create the main window
	SDL_Window* window = nullptr;
	if ( !_fullScreen )
	{
		window = SDL_CreateWindow(_name,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		_width,
		_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	}
	else
	{
		window = SDL_CreateWindow(_name,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		_width,
		_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
	}

	// Did window creation go wrong?
	if(!window)
	{
		NEP_LOG("ERROR: Couldn't create main window");
		SDL_Quit();
		return nullptr;
	}

	return static_cast<WindowHandle>( window );
}

void DisplayDeviceInterface::DestroyWindow(WindowHandle handle)
{
	SDL_Window* window = static_cast<SDL_Window*>( handle );
	SDL_DestroyWindow( window );
}

DisplayDeviceInterface::GraphicalContextHandle DisplayDeviceInterface::CreateGraphicalContext(WindowHandle window, u8 _openGLMinorVersion, u8 _openGLMajorVersion, GraphicalContextSettings _userSettings /*= GraphicalContextSettings()*/)
{
	// Platform specifics
	SDL_Window* win = static_cast<SDL_Window*>( window );

	// Create an OpenGL context and attach it to the window
	SDL_GLContext context = SDL_GL_CreateContext( win );

	if(!InitContext()) // Platform specific
		return nullptr;

	// Synchronize buffer swapping with the screen's refresh rate
	if (SDL_GL_SetSwapInterval(1) == -1) // Use VSync
		NEP_LOG("Warning DisplayDeviceInterface::CreateGraphicalContext - Swap interval not supported.");

	// Create and bind custom or default frame buffers
	if (DidUserTryToEnableOffScreenRendering(_userSettings))
		CreateFBOAndEnableReversedZIfNeeded(_userSettings);

	// Enable rendering options
	if (_userSettings.m_antiAliasing != MULTI_SAMPLE_ANTI_ALLIASING::NONE)	// Set anti alliasing
		glEnable(GL_MULTISAMPLE);

	glEnable(GL_DEPTH_TEST);												// Enables depth test
	if (!_userSettings.m_enableReversedZ)
		glDepthFunc(GL_LESS);												// Accepts fragment if closer to the camera than the former one. z = 0 at camera's position
	else
		glDepthFunc(GL_GREATER);											// Accepts fragment if closer to the camera than the former one. z = 0 at far plane's position
	glDepthRange(0.0, 1.0);													// Specify that depth-test-values must be between 0.0 and 1.0

	return static_cast<GraphicalContextHandle*>( context );
}

void DisplayDeviceInterface::DestroyGraphicalContext(GraphicalContextHandle handle)
{
	SDL_GLContext* context = static_cast<SDL_GLContext*>( handle );
	SDL_GL_DeleteContext( context );
}

void DisplayDeviceInterface::ClearBuffers(float backGroundColor[4])
{
	//glClearBufferfv( GL_COLOR, 0, backGroundColor );
	//glClearDepth(0.0f); // 0.0f instead of 1.0f because reversed-z is used
	//glClear(GL_DEPTH_BUFFER_BIT);

	// If off-screen rendering is used
	if (s_frame_buffer_object_index != FRAME_BUFFER_OBJECT_UNDEFINED)
	{
		NEP_GRAPHICS_ASSERT();

		glBindFramebuffer(GL_FRAMEBUFFER, s_frame_buffer_object_index);
		NEP_GRAPHICS_ASSERT();

		glClearBufferfv(GL_COLOR, 0, backGroundColor);
		NEP_GRAPHICS_ASSERT();

		glClearDepth(s_clear_depth_value); // 0.0f instead of 1.0f because reversed-z is used
		NEP_GRAPHICS_ASSERT();

		glClear(GL_DEPTH_BUFFER_BIT);
		NEP_GRAPHICS_ASSERT();

	}
}

void DisplayDeviceInterface::SwapBuffer(WindowHandle handle)
{
	// Blit off-screen-frame-buffer to back buffer 
	if (s_frame_buffer_object_index != FRAME_BUFFER_OBJECT_UNDEFINED)
	{
		NEP_GRAPHICS_ASSERT();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, s_frame_buffer_object_index);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
		glBlitFramebuffer(
			0, 0, s_frame_buffer_width, s_frame_buffer_height,
			0, 0, s_window_width, s_window_height,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		NEP_GRAPHICS_ASSERT();

		glBindFramebuffer(GL_FRAMEBUFFER, s_frame_buffer_object_index); // set custom frame-buffer as default render-zone
		NEP_GRAPHICS_ASSERT();

	}

	// Swap buffer
	SDL_Window* window = static_cast<SDL_Window*>( handle );
	SDL_GL_SwapWindow( window );
}