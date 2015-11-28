namespace Random
{

static unsigned long v = 1;
static unsigned long u = 2;

static void Srand( unsigned seed_ )
{
	v = seed_;
	u = seed_ + 1;
}

static unsigned long Rand()
{
	v = 36969 * ( v & 65535 ) + ( v >> 16 );
	u = 18793 * ( u & 65535 ) + ( u >> 16 );
	return ( v << 16 ) + u;
}

static unsigned long pRand( int v_ = 0, int u_ = 0 )
{
	static unsigned long v = 1;
	static unsigned long u = 2;
	if ( v_ && u_ )
	{
		v = v_;
		u = u_;
	}
	v = 36969 * ( v & 65535 ) + ( v >> 16 );
	u = 18793 * ( u & 65535 ) + ( u >> 16 );
	return ( v << 16 ) + u;
}

} // namespace Random
