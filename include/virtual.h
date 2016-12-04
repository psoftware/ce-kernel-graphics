#ifndef GL_VIRT_
#define GL_VIRT_

extern "C" void __cxa_pure_virtual()
{
	flog(LOG_INFO, "Pure Virtual function called, aborting.");
}

#endif