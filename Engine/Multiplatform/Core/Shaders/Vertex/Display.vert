#version 430 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 v_color;

uniform mat4 ModelView;

void main()
{
	v_color = color;
	gl_Position = ModelView * vec4( position, 1.0 );
}