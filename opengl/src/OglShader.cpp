// OglShader.cpp: implementation of the COglMesh class.
//
//////////////////////////////////////////////////////////////////////


#ifdef WIN32
#include <Windows.h>
#include <gl/GL.h>
#endif

//#include <Kernel/OpenGLib/glu.h> // LIMBO not picking up from the precompiled header in 64 bit
#include <OglShader.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <filesystem>

using namespace std;

#define	RETURN_IF_NOT_FIRST_TIME(a) static bool first=true; if(!first) return a; first = false;
#define GL_ASSERT
#define HDTIMELOG(X)
//#define CHECK_GLSL_STATE
#define GL_IGNORE_ERROR

//#define GLSL_VERBOSE

#ifdef OPENGLIB_EXPORTS //meaning we're building this as part of the FF OpenGLib.dll
#include <Kernel/OpenGLib/BitmapUtil.h>
#include <Kernel/WinWrapper/IGUIWrapper.h>
#include <Kernel/Config/PreferencesUtil.h>
#include <Kernel/MemoryManager/ApplicationMemoryApi.h>

// Avoid compiler warnings about the difference between
// size_t (unsigned int64) and unsigned int
#ifndef COUNTOF
#define COUNTOF( array ) ((unsigned int)(_countof( array )))
#endif

using namespace FS;

bool getPrimitiveOglModeDefault()
{
	//if (ApplicationMemoryStateGetRunningName() == ST_RUNNING_APPLICATION_NAME_VIEWER) // VIEWER_FIXES
	//	return true;

	return false;
}

ADD_PREF_SERIALIZED(RunPrimitiveOglMode, (bool)getPrimitiveOglModeDefault());
ADD_PREF_SERIALIZED(HighQualityEnvFileName, StringT(_T("Patterns\\seaworld.dds")))

#endif


#define assert_variable_not_found //assert(!"variable not found")

COglArg::COglArg(int val)
{
    type = eInt;
    ival = val;
}

COglArg::COglArg(float val)
{
    type = eFloat;
    fval = val;
}

COglArg::COglArg(float* val, int numFloats)
{
    switch(numFloats)
    {
    case 1 : type = eFloat ;  fval = *val;                break;
    case 2 : type = eFloat2;  allocFloat(val,numFloats);  break;
    case 3 : type = eFloat3;  allocFloat(val,numFloats);  break;
    case 4 : type = eFloat4;  allocFloat(val,numFloats);  break;
    case 16: type = eFloat16; allocFloat(val,numFloats);  break;
    default: assert(0); 
    }
}

void  COglArg::set(int val)
{
    if( type != eInt )
    {
        assert(!"type error");
        return;
    }
    ival = val;
}

void  COglArg::set(float val)
{
    if( type != eFloat )
    {
        assert(!"type error");
        return;
    }
    fval = val;
}

void  COglArg::set(float* val)
{
    switch(type)
    {
    case eFloat :  fval = *val;                            break;
    case eFloat2:  memcpy( fvalptr, val, sizeof(float)*2) ; break;
    case eFloat3:  memcpy( fvalptr, val, sizeof(float)*3) ; break;
    case eFloat4:  memcpy( fvalptr, val, sizeof(float)*4) ; break;
    case eFloat16: memcpy( fvalptr, val, sizeof(float)*16); break;
    default: assert(0);
    }
}

int COglArg::getInt()
{
    if( type != eInt )
    {
        assert(0);
        return 0;
    }
    return ival;
}

float COglArg::getFloat()
{
    if( type != eFloat )
    {
        assert(0);
        return 0.0f;
    }
    return fval;
}

float* COglArg::getFloatPtr()
{
    if( type < eFloat2 || type > eFloat16 )
    {
        assert(0);
        return 0;
    }

    return fvalptr;
}

float COglArg::getFloatAt(int idx)
{
    switch( type )
    {
    case eFloat2:  if( idx >= 0 && idx < 2 ) return fvalptr[idx]; else { assert(!"range error"); }  break;
    case eFloat3:  if( idx >= 0 && idx < 3 ) return fvalptr[idx]; else { assert(!"range error"); }  break;
    case eFloat4:  if( idx >= 0 && idx < 4 ) return fvalptr[idx]; else { assert(!"range error"); }  break;
    case eFloat16: if( idx >= 0 && idx < 16) return fvalptr[idx]; else { assert(!"range error"); }  break;
    default: assert(0); 
    } 

    return 0.0f;
}

void COglArg::allocFloat(float* val, int numFloats)
{
    fvalptr = new float[numFloats];
    memcpy( fvalptr, val, sizeof(float)*numFloats);
}

COglArg::~COglArg()
{
    if( type >= eFloat2 || type <= eFloat16 )
        delete [] fvalptr;
}


bool COglShader::mEnabled = true; //user override of global shader usage

COglShader::COglShader()
    : m_Error(false)
	, m_TextureUnitStates(0)
    , m_DefaultsLoaded(false)
    , m_Bound(false)
    , m_GeomShaderInType(GL_TRIANGLES)
    , m_GeomShaderOutType(GL_TRIANGLES)
{
}

COglShader::~COglShader()
{
    HLOG(_T("~COglShader()"));
#if TEXTURE_SUPPORT
    clearTextures();
#endif
}

bool COglShader::isEnabled()
{
    return mEnabled;
}

void COglShader::enable(bool set)
{
    mEnabled = set;
}

void COglShader::setVariablei( const std::string& name, int  value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    auto it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert(make_pair(name, make_shared<COglArg>(value)));
    else
        it->second->set(value);

    if(m_Bound)
    {
		int progId = programID();
		int dbg = glGetUniformLocationARB(progId, "DepthPeel");
        int argLocation = glGetUniformLocationARB( progId, name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform1iARB( argLocation, value ); GL_ASSERT;
        }
    }
}

void COglShader::setVariable( const std::string& name, float  value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( value ) ) );
    else
        it->second->set(value);

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform1fARB( argLocation, value ); GL_ASSERT;
        }
    }
}

void COglShader::setVariable( const std::string& name, col4f& value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( (float*)value, 4 ) ) );
    else
        it->second->set(value);

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform4fARB( argLocation, value.r, value.g, value.b, value.o );  GL_ASSERT;
        }
    }
}

void COglShader::setVariable( const std::string& name, m44f&  value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( value.transposef(), 16 ) ) );
    else
        it->second->set( value.transposef() );

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniformMatrix4fv( argLocation, 1, false, value.transposef() );  GL_ASSERT;
        }
    }

}

void COglShader::setVariable( const std::string& name, p4f&   value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( (float*)value, 4 ) ) );
    else
        it->second->set(value);

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform4fARB( argLocation, value.x, value.y, value.z, value.w ); GL_ASSERT;
        }
    }
}

void COglShader::setVariable( const std::string& name, p3f&   value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( (float*)value, 3 ) ) );
    else
        it->second->set(value);

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform3fARB( argLocation, value.x, value.y, value.z );  GL_ASSERT;
        }
    }
}

void COglShader::setVariable( const std::string& name, p2f&   value )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    ArgMapType::iterator it;

    it = m_ArgumentMap.find( name );
    if( it == m_ArgumentMap.end() )
        m_ArgumentMap.insert( ArgMapType::value_type( name, new COglArg( (float*)value, 2 ) ) );
    else
        it->second->set(value);

    if(m_Bound)
    {
        int argLocation = glGetUniformLocationARB( programID(), name.c_str()); GL_ASSERT;
        if( argLocation >= 0 )
        {
            glUniform2fARB( argLocation, value.x, value.y );  GL_ASSERT;
        }
    }
}

#if HAS_SHADER_SUBROUTINES
void COglShader::setFragSubRoutine(const char* name)
{
	GLuint subIdx = glGetSubroutineIndex(programID(), GL_FRAGMENT_SHADER, name);
	if (subIdx >= 0) {
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &subIdx);
	}
}

void COglShader::setVertSubRoutine(const char* name)
{
	GLuint subIdx = glGetSubroutineIndex(programID(), GL_VERTEX_SHADER, name);
	if (subIdx >= 0) {
		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &subIdx);
	}
}
#endif

#if TEXTURE_SUPPORT
void   clearTextures();
void COglShader::clearTextures()
{
    m_TextureMap.clear();
}

bool COglShader::setTexture( const std::string& textureShaderName, const std::string& filename, bool flipImage )
{
    HLOG(Format(_T("COglShader::setTexture %s %s"),textureShaderName,filename));

    auto iter = m_TextureMap.find( textureShaderName );
    if(iter != m_TextureMap.end() )
    {
        if(iter->second && (iter->second->mOwner == this) )
        {
            HLOG(Format(_T("deleting texture %s, id %d in in shader"),textureShaderName,map->second->m_hwID));
            iter->second = 0;
        }
    }

	if( filename.empty() )
	{
		m_TextureMap.insert( TextureMapType::value_type( textureShaderName, (COglTexture *)0 ) );
		return true;
	}

    bool dds=false;
    {
        //LIMBO use check extension utility
        string name( filename );
//        name = name.MakeLower();
        dds = name.find(".dds") > 0;
    }
#if 0
    CTextureLoader* img = 0;
    if(dds)
        img = new CDDSImage;

#ifdef OPENGLIB_EXPORTS
    else
        img = new Bitmap;
#endif

    if( !img || !img->load( filename, flipImage) )
    {
        delete img;
        return false;
    }

    img->mOwner = this; // this shader will delete this texture

    m_TextureMap.insert( TextureMapType::value_type( textureShaderName, img ) );
#endif
    return true;
}

bool COglShader::hasTexture ( const std::string& textureShaderName)
{
    TextureMapType::iterator target = m_TextureMap.find( textureShaderName );
    return target != m_TextureMap.end();
}

bool COglShader::setTexture ( const std::string& textureShaderName, COglTexture* texture, bool takeOwnerShip )
{
    // HLOG(Format(_T("COglShader::setTexture %s %s"),textureShaderName, takeOwnerShip ? _T("shader will delete") : _T("shader will NOT delete")));

    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    if( !texture )
    {
        TextureMapType::iterator target = m_TextureMap.find( textureShaderName );
        if( target != m_TextureMap.end() )
        {
            if( target->second && (target->second->mOwner == this ))
            {
                HLOG(Format(_T("deleting texture %s, id %d in in shader"),textureShaderName,target->second->m_hwID));
                delete target->second;
				target->second = 0;
            }
        }

		m_TextureMap.insert( TextureMapType::value_type( textureShaderName, (COglTexture *)0 ) );
    }
    else
    {
        TextureMapType::iterator target = m_TextureMap.find( textureShaderName );
        if( target != m_TextureMap.end() )
        {
            if( target->second && (target->second->mOwner == this) )
            {
                HLOG(Format(_T("deleting texture %s, id %d in in shader"),textureShaderName,target->second->m_hwID));
                delete target->second;
            }

            target->second = texture;
        }
        else
        {
            if( takeOwnerShip )
            {
                assert( !texture->mOwner );
                texture->mOwner = this; // this shader will delete this texture
            }

            m_TextureMap.insert( TextureMapType::value_type( textureShaderName, texture ) );
        }
    }

    return true;
}

COglTexture* COglShader::getTexture( const std::string& textureShaderName )
{
    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    TextureMapType::iterator target = m_TextureMap.find( textureShaderName );
    assert(target != m_TextureMap.end() && "shader not initialized or texture name incorrect");

    if( target != m_TextureMap.end() )
        return target->second;     

    return 0;
}
#endif

#define GET_EXT_POINTER(name, type) name = (type)wglGetProcAddress(#name)

namespace
{

    bool checkOglExtension(const char* name)
    {
        static GLint numExt;
        static set<string> names;
        if (numExt == 0) {

            glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
            for (GLint i = 0; i < numExt; i++) {
                const GLubyte* pName = COglShader::glGetStringi(GL_EXTENSIONS, i);
            }
        }
    }
}

bool COglShader::hasShaderError(int obj)
{
#ifdef GLSL_VERBOSE
    int infologLength = 0;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

	if( infologLength > 0 )
	{
		int charsWritten  = 0;
		char* err = new char[infologLength];
	 
		glGetInfoLogARB( obj, infologLength, &charsWritten, err );
		err[charsWritten] = '\0';

		string wErr = string(err);

//		cout << err << endl;
		HLOG( Format( _T("compiling shader \"%s\":\n%s\n\n"),(const std::string&)m_Name, wErr));
		delete [] err;
	}
#endif
	return false;
}

bool COglShader::hasProgramError(int obj)
{
#ifdef GLSL_VERBOSE
    int infologLength = 0;	
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if( infologLength > 0 )
	{
		int charsWritten  = 0;
		char* err = new char[infologLength];
		glGetProgramInfoLog(obj, infologLength, &charsWritten, err);
		err[charsWritten] = '\0';

		string wErr = string(err);
		HLOG( Format( _T("linking shader \"%s\":\n%s"),(const std::string&)m_Name, wErr) );

		delete [] err;
	}
#endif
	return false;
}

string COglShader::getDataDir()
{
#ifdef OPENGLIB_EXPORTS
    return getGUIWrapper()->getExeDirectory().getBuffer();
#else
    return "";
#endif
}


//assumes an array of strings delimited with multiple newlines
static char* getNextLine(char* str, char* max)
{
    assert(str && str < max);
    if(!str || (str >= max))
        return 0;

    size_t offset = strlen(str);

    // in case we are on a line of no length
    // skip multiple nulls (which came from previous multiple endlines \r\n etc.)
    while( offset < 1 && ++str < max)
        offset = strlen( str );

    // if the end of this line is a null move forward
    // until it isn't... that will give us the next line
    while( !*(str + offset) && (++str + offset) < max );
        
    return (str + offset) >= max ? 0 : str + offset;
}

//assumes an array of strings delimited with multiple newlines
static char* getVariableName(char* str)
{
    assert(str && strlen(str));
    const unsigned int bufsize = 24;
    static char name[24];
    name[0] = '\0';
    sscanf_s(str, "%s", name, bufsize);
    
    size_t sz = strlen(name);
    if(name[sz-1]==';')
        name[sz-1] = '\0';

    return name;
}

static bool doesFileExist( const std::string& pathFile )
{
	if (pathFile.empty())
		return false;

    bool ok = filesystem::exists(pathFile);

	return ok;
}

static string getCheckedImagePath(const std::string& str)
{
    string fname = COglShader::getDataDir();

    if( !fname.empty() )
    {
        fname += string( str );

        if( !doesFileExist(fname) )
        {
            assert(!"default shader image file not found");
            fname = "";
        }
    }
    return fname;
}

void COglShader::loadDefaultVariables()
{
    m_DefaultsLoaded = true; 

    // LIMBO need to add vertex shader parsing? 
    // LIMBO Do we need more info in the header?
    // LIMBO Should set up a static texture cache for default textures
    // so we only have one default instance of each texture on the card
    for(int i=0; i<3; i++)
    {
        char* orgSource = 0;
        
        switch(i)
        {
        case 0: orgSource = getVertexShaderSource();   break;
        case 1: orgSource = getGeometryShaderSource(); break;
        case 2: orgSource = getFragmentShaderSource(); break;
        };
        
        if(!orgSource || !strlen(orgSource)) 
            continue;

        size_t end = strlen(orgSource);
        char* sourceCpy = new char[ end+1 ];
        strcpy_s(sourceCpy, end + 1,orgSource);
        sourceCpy[end] = '\0';
        char* source  = sourceCpy; // don't change the original source
        char* endChar = &source[end];

	    //make all newlines endlines
	    for(int i=0; i<end; i++)
		    if(source[i] == '\n' || source[i] == '\r')
			    source[i] = '\0';

        do
        {
            // check for uniform variable declaration containing the STI //* comment tag
			bool stiDefaultVariable = strstr( source, "//*") ? true : false;
            if( !strncmp( source, "uniform", 7 ) )
            {
                char* tname  = 0;
				char* stiArg = 0;

                string vname;

				if( stiDefaultVariable )
				{
					stiArg = strstr( source, "//*" ) + 3;

					while( stiArg[0] == ' ' || stiArg[0] == '\t' )    
						stiArg++;
				}

                if( stiDefaultVariable && (tname = strstr( source, "float" )) )
                {
                    vname = getVariableName( tname + 5 );
                    setVariable( vname, (float)atof( stiArg ));
                }
                else if( stiDefaultVariable && (tname = strstr( source, "vec2" )) )
                {
                    vname = getVariableName( tname + 4 );
                    p2f val;
                    int num = sscanf_s( stiArg, "%f %f", &val.x, &val.y ); 
                    assert(num == 2 && "missing shader defaults");

                    setVariable( vname, val);
                }
                else if( stiDefaultVariable && (tname = strstr( source, "vec3" )) )
                {
                    vname = getVariableName( tname + 4 );
                    p3f val;
                    int num = sscanf_s( stiArg, "%f %f %f", &val.x, &val.y, &val.z );
                    assert(num == 3 && "missing shader defaults");

                    setVariable( vname, val);
                }
                else if( stiDefaultVariable && (tname = strstr( source, "vec4" )) )
                {
                    vname = getVariableName( tname + 4 );
                    col4f col;
                    int num = sscanf_s( stiArg, "%f %f %f %f", &col.r, &col.g, &col.b, &col.o );
                    assert(num == 4 && "missing shader defaults");

                    setVariable( vname, col);
                }
#if TEXTURE_SUPPORT
                else if( tname = strstr( source, "sampler2DRect" ) )
                {
                    vname = getVariableName( tname + 14 );
					if( !stiArg )
					{
						setTexture( string(vname), 0 );
					}
                    else if( strstr( stiArg, "GraphicsRGBABuffer") )
                    {
                        setTexture( vname, new COglGraphicsColorBuffer, true );
                    }
                    else if( strstr( stiArg, "GraphicsDepthBuffer") )
                    {
                        setTexture( vname, new COglGraphicsDepthBuffer(), true );
                    }
                    else if( strstr( stiArg, "SystemRGBABuffer") )
                    {
                        setTexture( vname, new COglSystemImageBuffer(GL_BGRA_EXT), true );
                    }
                    else if( strstr( stiArg, "SystemAlphaBuffer") )
                    {
                        setTexture( vname, new COglSystemImageBuffer(GL_ALPHA), true );
                    }
                    else
                    {
                        string path = getCheckedImagePath( stiArg );
                        if( path.GetLength() )
                            setTexture( string(vname), path );
						else
							setTexture( string(vname), 0 ); // need to make sure the texture is present and gets a slot for ATI
                    }
                }
                else if( tname = strstr( source, "sampler2D" ) )
                {
                    // we may be using pow2 image buffers in the future
                    // also we should verify the po2ness of the file and trigger a resize if needed
                    vname = getVariableName( tname + 9 );

					if( !stiArg )
					{
						setTexture( string(vname), 0 );
					}
					else
					{
						string path = getCheckedImagePath( stiArg );
						if( path.GetLength() )
							setTexture( string(vname), path );
						else
							setTexture( string(vname), 0 ); // need to make sure the texture is present and gets a slot for ATI
					}
                }
                else if( tname = strstr( source, "samplerCube" ) )
                {
                    vname = getVariableName( tname + 11 );

					if( !stiArg )
					{
						setTexture( string(vname), 0 );
					}
					else
					{
						string path;
#ifdef OPENGLIB_EXPORTS // e.g. FF not StlViewer
						StringT prefName = getPrefEntry<StringT>(_T("HighQualityEnvFileName"));
						if (prefName.isEmpty())
							path = getCheckedImagePath( stiArg );
						else {
							StringC str = toStringC(prefName);
							path = getCheckedImagePath( str.getBuffer() );
						}
#else
						path = getCheckedImagePath( stiArg );
#endif
						if( path.GetLength() )
							setTexture( string(vname), path );
						else
							setTexture( string(vname), 0 ); // need to make sure the texture is present and gets a slot for ATI
					}
				}
#endif
                else if( stiDefaultVariable )
                {
                    assert(!"unknown type");
                    continue;
                } 
            }
        } while( source = getNextLine(source, endChar) );

        delete [] sourceCpy;
    }
}

#define LOG_AND_RETURN(a,b) if(a) { m_Error = true; m_Log = b; return false;}

bool COglShader::load()
{
    HDTIMELOG("Entering COglShader::load()");
    CHECK_GLSL_STATE;

    // Get Vertex And Fragment Shader Sources
    char * isource = getShaderIncludeSource();
    char * fsource = getFragmentShaderSource();
    char * vsource = getVertexShaderSource();
    char * gsource = getGeometryShaderSource();

    // insert the 'shader include source' in the fragment source
    // we may want to extend this in the future
    size_t len = isource ? strlen(isource)+strlen(fsource)+1 : strlen(fsource);
    char* source = new char[len+1];

#pragma warning(push)
#pragma warning(disable: 4996)
    if(isource)
    {
        strcpy(source, isource);
        strcpy(&source[strlen(isource)], fsource);
    }
    else
        strcpy(source,fsource);

#pragma warning(pop)
    source[len] = '\0';

    LOG_AND_RETURN( !source || !vsource, "Failed to access shader code" )

    if( !m_DefaultsLoaded ) 
        loadDefaultVariables();

    // Create Shader And Program Objects
    int& progID = programID();
    int& vertID = vertexID (); 
    int& fragID = fragmentID();
    int& geomID = geometryID();

    progID  = glCreateProgramObjectARB();
    vertID  = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    fragID  = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    geomID  = gsource ? glCreateShaderObjectARB(GL_GEOMETRY_SHADER_EXT) : 0;

    LOG_AND_RETURN( !progID || !vertID || !fragID, "Shader compilation failed" )

    if(gsource && !geomID)
        LOG_AND_RETURN( gsource && !geomID , "Your card and or driver does not support geometry shaders")

    // Load Shader Sources
    glShaderSourceARB( vertID, 1, (const GLcharARB**)&vsource, NULL); GL_ASSERT;
    glShaderSourceARB( fragID, 1, (const GLcharARB**) &source, NULL); GL_ASSERT;

    if( gsource && geomID )
        glShaderSourceARB( geomID, 1, (const GLcharARB**)&gsource, NULL); GL_ASSERT;

    // Compile The Shaders
    glCompileShaderARB( vertID); 
	hasShaderError( vertID );

    glCompileShaderARB( fragID );   
	hasShaderError( fragID );

    if( gsource && geomID )
	{
        glCompileShaderARB( geomID );
		hasShaderError( geomID );
	}

    // Attach The Shader Objects To The Program Object
    glAttachObjectARB( progID, vertID );   GL_ASSERT;
    glAttachObjectARB( progID, fragID );   GL_ASSERT;

    if( gsource && geomID && glProgramParameteriEXT )
    {
        glAttachObjectARB( progID, geomID ); GL_ASSERT;

		glProgramParameteriEXT(progID, GL_GEOMETRY_INPUT_TYPE_EXT,  m_GeomShaderInType  );
		glProgramParameteriEXT(progID, GL_GEOMETRY_OUTPUT_TYPE_EXT, m_GeomShaderOutType );

		int temp;
		glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT,&temp);
		glProgramParameteriEXT(progID, GL_GEOMETRY_VERTICES_OUT_EXT,temp);
    }

    // Link The Program Object
    glLinkProgramARB( progID );  
    delete [] source; // NB: living with the leak if we have an error condition

	initUniform();
    HDTIMELOG("Succesfully completed COglShader::load()");
    return true;
}

 void COglShader::setGeomShaderIOtype( int intype, int outtype)
 {
    m_GeomShaderInType = intype;
    m_GeomShaderOutType = outtype;
 }

bool COglShader::unLoad()
{
    CHECK_GLSL_STATE;
    return true;
}

bool COglShader::bind()
{
    GL_IGNORE_ERROR;    //ignore any rogue errors from other areas of the system
    CHECK_GLSL_STATE;
    
    assert(!m_Bound); // logic error all shaders must be bound and unbound 

    int& progID = programID();
    if( progID && !m_Error )
    {
        glUseProgramObjectARB( progID ); // Use The Program Object Instead Of Fixed Function OpenGL
        hasProgramError( progID );       // this can show additional errors...
    }

    else if(m_Error)              // some error state, so stop trying to load or bind
        glUseProgramObjectARB(0); // Fixed Function OpenGL
   
    else // must be first time, shader needs loading
    {
        if( !load() )
            return false;
   
        else
        {
            glUseProgramObjectARB( progID ); // Use The Program Object Instead Of Fixed Function OpenGL

            if( hasProgramError( progID ) )
            {
                assert(!"shader program error");
                return false;
            }

            GL_ASSERT; // seems like the above is no longer catching fail conditions, e.g. simple glsl compile errors
        }
    }     

#if TEXTURE_SUPPORT
    //bind all the textures now 
    if( m_TextureMap.size() )
    {
        assert( !m_TextureUnitStates ); // texture states should have been destroyed at end of each render pass
        m_TextureUnitStates = new ActiveTextureUnits;

		int texUnit = -1;

        // taking 2 passes so the textures with matrices get the first 8 slots
        // texture slots 8-16 do not have matrices
        for( int i=0; i<2; i++ ) 
        for( TextureMapType::iterator mp = m_TextureMap.begin(); mp != m_TextureMap.end(); mp++ )
        {
            string textureName  =  mp->first;
            COglTexture* texture =  mp->second;

            if( !i && (texture && texture->textureMatrix().isIdentity()) )
                continue;

            texUnit++;
			int type = texture ? texture->get_type() : GL_TEXTURE_2D;
			int hwid = texture ? texture->m_hwID : 0;

            m_TextureUnitStates->activateUnit( texUnit, type );               GL_ASSERT;

			if( texture )
			{
				texture->m_textureShaderUnit = texUnit;
				texture->bind();                                                   GL_ASSERT;

				// texture matrix 
				if( texUnit < 8 ) // as of GTX280 anno end 2009 there are only 8 slots for texture matrices
				{
					glMatrixMode(GL_TEXTURE); // verify that this works correctly for independent texture slots
					glLoadIdentity();
					glMultMatrixf( texture->textureMatrix() );
					glMatrixMode(GL_MODELVIEW);													GL_ASSERT;
				}
			}

            int samplerLocation = glGetUniformLocationARB( progID, Uni2Ansi(textureName));  GL_ASSERT;
            
            HLOG(Format("Bound texture %s unit: %d loc: %d hwid: %d",(const std::string&)textureName, texUnit, samplerLocation, hwid ));

            if( samplerLocation >= 0 )
            {
                glUniform1iARB(samplerLocation, texUnit);    GL_ASSERT;
            }
        }
    }
#endif

    //send all the arguments
    if(m_ArgumentMap.size())
    {
        for( ArgMapType::iterator arg = m_ArgumentMap.begin(); arg != m_ArgumentMap.end(); arg++ )
        {
            string argName =  arg->first;
            COglArg& argVal  = *arg->second;

            int argLocation = glGetUniformLocationARB( programID(), argName.c_str()); GL_ASSERT;

            if( argLocation >= 0 )
            {
                /* This would be good to be able to switch on per shader
                string argval;
                switch(argVal.getType() )
                {
                case COglArg::eFloat: argval = Format(" %.1f",argVal.getFloat());break;
                case COglArg::eFloat3:
                    {
                        float* val=argVal.getFloatPtr();
                        argval = Format(" %.1f %.1f %.1f",val[0],val[1],val[2]);break;
                    }
                case COglArg::eFloat4:
                    {
                        float* val=argVal.getFloatPtr();
                        argval = Format(" %.1f %.1f %.1f %.1f",val[0],val[1],val[2],val[3]);break;
                    }
                }
                HLOG(Format("Binding argName:%s val:%s",(const std::string&)argName,(const std::string&)argval));
                */

                switch( argVal.getType() )
                {
                case COglArg::eInt: 
                    glUniform1iARB( argLocation, argVal.getInt() );
                    break;
                case COglArg::eFloat:
                    glUniform1fARB( argLocation, argVal.getFloat() ); 
                    break;
                case COglArg::eFloat2: 
                    glUniform2fARB( argLocation, argVal.getFloatAt(0),  argVal.getFloatAt(1)); 
                    break;
                case COglArg::eFloat3:
                    glUniform3fARB( argLocation, argVal.getFloatAt(0),  argVal.getFloatAt(1), argVal.getFloatAt(2)); 
                    break;
                case COglArg::eFloat4:
                    glUniform4fARB( argLocation, argVal.getFloatAt(0),  argVal.getFloatAt(1), argVal.getFloatAt(2), argVal.getFloatAt(3)); 
                    break;
                case COglArg::eFloat16:
                    glUniformMatrix4fv( argLocation, 1, false, argVal.getFloatPtr());   
                    break;
                default: assert(0);
                }
            }
        }
    }

    m_Bound = true;
    return m_Error;
}

bool COglShader::getVariable( const std::string& name, string& type, string& value )
{
    bool found = false;

    if(m_ArgumentMap.size())
    {
        ArgMapType::iterator arg = m_ArgumentMap.find( name );
        stringstream ss;
        if( arg != m_ArgumentMap.end() )
        {
            found = true;
            COglArg& argVal = *arg->second;
            switch( argVal.getType() )
            {
            case COglArg::eInt: 
                assert(!"implement type");
                break;
            case COglArg::eFloat:
                type = "float";  // TBD: should share this string with COglMaterial
                ss << argVal.getFloat();
                break;
            case COglArg::eFloat2: 
                assert(!"implement type"); 
                break;
            case COglArg::eFloat3:
                assert(!"implement type");
                break;
            case COglArg::eFloat4:
                type = "color";  // TBD: should share this string with COglMaterial
                ss << argVal.getFloatAt(0) << " " << argVal.getFloatAt(1) << " " << argVal.getFloatAt(2) << " " << argVal.getFloatAt(3);
                break;
            case COglArg::eFloat16:
                type = "m44f";  // TBD: should share this string with COglMaterial
                for(int i=0; i<16; i++) {
                    ss << argVal.getFloatAt(i);
                    if (i != 15)
                        ss << " ";
                }
                break;
            default: assert(0);
            }
            value = ss.str();
        }
    }

    return found;
}

bool COglShader::getVariablei( const std::string& name, int& value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eInt );
        
		value = argVal.getInt();
        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, float& value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat );
        
        value = argVal.getFloat();
        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, col4f& value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat4 );

        float* flt = argVal.getFloatPtr();
        value.set( flt[0], flt[1], flt[2], flt[3] );

        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, m44f&  value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat16 );

        value.set( argVal.getFloatPtr() );

        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, p4f&   value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat4 );

        float* flt = argVal.getFloatPtr();
        value.set( flt[0], flt[1], flt[2], flt[3] );

        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, p3f&   value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat3 );

        float* flt = argVal.getFloatPtr();
        value.set( flt[0], flt[1], flt[2]);

        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::getVariable( const std::string& name, p2f&   value )
{
    ArgMapType::iterator arg = m_ArgumentMap.find( name );
    if( arg != m_ArgumentMap.end() )
    {
        COglArg& argVal = *arg->second;
        assert( argVal.getType() == COglArg::eFloat2 );        

        float* flt = argVal.getFloatPtr();
        value.set( flt[0], flt[1]);

        return true;
    }

    assert_variable_not_found;
    return false;
}

bool COglShader::unBind()
{    
    CHECK_GLSL_STATE;
    assert(m_Bound); // logic error can't unbind an unbound shader

    glUseProgramObjectARB(0);   GL_ASSERT; // Back to Fixed Function OpenGL

#if TEXTURE_SUPPORT
	//reset all the states of all the texture units
	if( m_TextureMap.size() )
	{
		assert( m_TextureUnitStates );
		delete m_TextureUnitStates; //the destructor fires the state cleanup code
		m_TextureUnitStates = 0;
	}
#endif

    m_Bound = false;   
    return true;
}
