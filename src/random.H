#include <stdint.h>

namespace Random
{

static uint32_t v = 1;
static uint32_t u = 2;

static void Srand( uint32_t seed_ )
{
	v = seed_;
	u = seed_ + 1;
}

static void Srand( uint32_t seed_, uint32_t seed2_ )
{
	v = seed_;
	u = seed2_;
}

static uint32_t Rand()
{
	v = 36969 * ( v & 65535 ) + ( v >> 16 );
	u = 18793 * ( u & 65535 ) + ( u >> 16 );
	return ( v << 16 ) + u;
}

static uint32_t Seed( uint32_t& seed2_ )
{
	seed2_ = u;
	return v;
}

static void pSrand( uint32_t seed_ )
{
	srand( seed_ );
}

static uint32_t pRand()
{
	return random();
}

} // namespace Random
