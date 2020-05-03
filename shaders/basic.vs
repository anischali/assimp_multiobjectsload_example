#version 330

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform float texRepeat;

layout (location = 0) in vec3 vsiPosition;
layout (location = 1) in vec3 vsiNormal;
layout (location = 2) in vec2 vsiTexCoord;
 
out vec2 vsoTexCoord;
out vec3 vsoNormal;
out vec4 vsoModPosition;

uniform int complex_object;
void main(void) {
  if (complex_object == 1){
    mat4 modelViewMatrix = viewMatrix * modelMatrix;
    vsoNormal = (transpose(inverse(modelViewMatrix)) * vec4(vsiNormal.xyz, 0.0)).xyz;
    vsoModPosition = modelViewMatrix * vec4(vsiPosition.xyz, 1.0);
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vsiPosition.xyz, 1.0);
    vsoTexCoord = vec2(vsiTexCoord.x, 1.0 - vsiTexCoord.y);
    
  }else{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vsiPosition.xyz, 1.0);
    vsoTexCoord = texRepeat * vsiTexCoord;

  }
}
