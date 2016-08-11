#include <cstdio>
#include <cstdlib>
#include <cstdarg>

class Debug
{
public:
	Debug( const char *name_ = 0 ) : _name( 0 )
	{
		if ( !name_ )
			name_ = getenv( "FL_DEBUG" );
		if ( name_ && name_[ 0 ] )
		{
			remove( name_ );
			_name = strdup( name_ );
		}
	}
	~Debug()
	{
		free( _name );
	}
	void write( const char *s_, ... )
	{
		if ( !_name )
			return;

		char buf[1024];
		va_list argp;
		va_start( argp, s_ );
		vsnprintf( buf, sizeof( buf ), s_, argp );
		va_end( argp );
		FILE *f = fopen( _name, "a" );
		if ( f )
		{
			fprintf( f, "%s\n", buf );
			fclose( f );
		}
	}
private:
	char *_name;
};

static Debug dbg;
#define LOG(x) { std::ostringstream os; os << "!" << x; dbg.write( os.str().c_str() ); }

#ifndef NDEBUG
#define DBG(x) { std::ostringstream os; os << x; dbg.write( os.str().c_str() ); }
#else // ifndef NDEBUG
#define DBG(x)
#endif // NDEBUG
