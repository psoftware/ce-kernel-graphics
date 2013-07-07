#include "tipo.h"
#include "allinea.h"
// restituisce il valore di "v" allineato a un multiplo di "a"
natl allinea(natl v, natl a)
{
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}
addr allineav(addr v, natl a)
{
	return (addr)allinea((natl)v, a);
}
// )
