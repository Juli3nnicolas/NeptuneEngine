#pragma once

#include "Graphics/Renderer.h"
#include "Graphics/Color.h"

#include <vector>
#include <unordered_map>


///
/// \Class		This class creates View instances. It is important to know that every instance
///				will share the same data, whether generated by one of the public methods or
///				passed by the user as a parameter. It is also important to note that the users
///				is responsible for managing their data and make sure they are available for
///				all views created by the same spawner.
///
/// \Note		This class can be seen as an highly coupled factory. In the sense that every produced
///				objects always depend on the factory and the parameters provided by the user to exist. 
///
/// \Attention	The layout position 0 in GLSL shaders is reserved for the vertices' position.
///
/// \Warning	Unless stated otherwise, all the graphics programs and graphics-program input-parameters provided to
///				this class are not copied. They are referenced using pointers. Meaning you are responsible for managing 
///				these data and keep them available (allocated) for this class at all times.	
///				For the sake of performance, this class will not manage any of your data.
///				If you want a simpler behavior, please have a look at the ViewFactory subclasses.
///

namespace Neptune
{
	class Texture;
	class View;

	class ViewSpawner
	{
	public:
		//
		// C O N S T R U C T O R S
		//
		
		ViewSpawner(const char* _pgmName, GraphicsProgram* _pgm);											/// Creates the spawner with a first graphics-program _pgm called _pgmName


		//
		// D E F A U L T   G E N E R A T E D   M E T H O D S
		//

		virtual ~ViewSpawner()								= default;
		virtual ViewSpawner& operator=(const ViewSpawner&)	= delete;
		ViewSpawner(const ViewSpawner&)						= delete;


		//
		// P U R E   V I R T U A L   M E T H O D S 
		//

		virtual bool  CreateColorData(const Color& _c)	=0;													/// Creates per-vertex color data, if the data were already created, the color will be changed (for all the view instantiated by the factory). The input color for this method is not referenced.
		virtual bool  CreateNormalData()				=0;													/// Creates the normals at every vertex of the view
		virtual bool  Create2DTextureMapData()			=0;													/// Creates the data to be able to map a 2D texture on the whole view


		//
		// C R E A T I O N   M E T H O D S 
		//

		View* create();																						/// Allocates the view on the heap


		//
		// G R A P H I C S - P R O G R A M   R E L A T E D   M E T H O D S 
		//

		void addGraphicsProgram(const char* _name,    GraphicsProgram* _pgm);								/// Add another graphics program, this will add another draw call for the current view
		void addShaderAttribute(const char* _pgmName, const GraphicsProgram::ShaderAttribute& _shaderAtt);	/// Add the shader attribute  _shaderAtt as an input for the program _pgmName. 
		void addUniformVariable(const char* _pgmName, const GraphicsProgram::UniformVarInput& _uniform);	/// Add the shader attribute  _shaderAtt as an input for the program _pgmName.

		bool MapColorData       (  const char* _pgmName, u8 _layout);										/// Add the spawner's color data as an input for the program _pgmName at GLSL layout position _layout
		bool MapNormalData      (  const char* _pgmName, u8 _layout);										/// Add the spawner's normal data as an input for the program _pgmName at GLSL layout position _layout
		bool Map2DTextureMapData(  const char* _pgmName, u8 _layout);										/// Add the spawner's 2d texture map coordinates as an input for the program _pgmName at GLSL layout position _layout


	protected:
		//
		// S T R U C T U R E S
		//

		struct Program
		{
			GraphicsProgram*			m_program;
			std::vector<const void*>	m_shaderAttributeIDs;	/// Attribute's position in m_shaderAttributes
			std::vector<const void*>	m_uniformVarIDs;		/// Uniform variable's position in m_uniformVariables
		};


		//
		// P U R E   V I R T U A L   M E T H O D S 
		//

		virtual void createVertexData()						=0;

		/// \brief In this method you must dynamically allocate a View object
		/// (called v). Then call v->getRenderer().setDrawingPrimitive(_prim)  
		/// and v->getRenderer().setNbverticesToRender(nb).
		virtual View* CreateViewAndSetUpRenderParameters()	=0;


		//
		// A T T R I B U T E S
		//

		std::vector<float>								m_vertices;
		std::vector<Color>								m_colors;
		std::vector<float>								m_2DTexCoords;
		std::vector<float>								m_normals;
		Texture*										m_texture;											/// Texture to be used in the default shaders. Probably will be refactored later
		
	private:
		std::unordered_map<const char*, Program>							m_programs;						/// Need to manually set the hash function
		std::unordered_map<const void*,GraphicsProgram::ShaderAttribute>	m_shaderAttributes;				/// Stores all the shader attributes for all the programs. The void* is the address of the buffer containing the data (m_data field).
		std::unordered_map<const void*,GraphicsProgram::UniformVarInput>	m_uniformVariables;				/// Stores all the uniform variables for all the programs. The void* is the address of the buffer containing the data (m_data field).
	};
}