varying vec3  Normal;
varying vec3  ObjPos;
varying vec3  RawObjPos;
varying vec4  Color;

uniform vec3 setBlockOffset;  //* 0.0 0.0 0.0

void main(void) 
{
    gl_Position    = ftransform();
    Normal         = normalize(gl_NormalMatrix * gl_Normal);
    ObjPos         = (gl_ModelViewMatrix * gl_Vertex).xyz;  
    RawObjPos	   = gl_Vertex.xyz + setBlockOffset;
    Color		   = gl_Color;
}  