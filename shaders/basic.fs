#version 330
uniform sampler2D tex;
uniform int border;
uniform int complex_object;

out vec4 fragColor;


uniform vec4 lumpos;

uniform sampler2D myTexture;
uniform int hasTexture;
uniform vec4 diffuse_color;
uniform vec4 specular_color;
uniform vec4 ambient_color;
uniform vec4 emission_color;
uniform float shininess;

in vec2 vsoTexCoord;
in vec3 vsoNormal;
in vec4 vsoModPosition;

void complex_geometry(void){
    vec3 lum  = normalize(vsoModPosition.xyz - lumpos.xyz);
    float diffuse = clamp(dot(normalize(vsoNormal), -lum), 0.0, 1.0);
    vec3 lightDirection = vec3(lumpos - vsoModPosition);

    vec4 specularReflection = specular_color * pow(max(0.0, dot(normalize(reflect(-lightDirection, vsoNormal)), normalize(vec3(-vsoModPosition)))), shininess);

    vec4 diffuseReflection = ambient_color*0.2 +diffuse_color * diffuse;
    fragColor = diffuseReflection + specularReflection;
    if(hasTexture != 0)
      fragColor *= texture(myTexture, vsoTexCoord);
   
}


void simple_geometry(void){
    if( border != 0 && (vsoTexCoord.s < 0.02 ||
		      vsoTexCoord.t < 0.02 ||
		      (1 - vsoTexCoord.s) < 0.02 || 
		      (1 - vsoTexCoord.t) < 0.02 ) )
    fragColor = vec4(0.5, 0, 0, 1);
  else
    fragColor = texture(tex, vsoTexCoord);
}



void main(void) {
  if (complex_object == 1){
    complex_geometry();
  }else{
    simple_geometry();
  }
}
