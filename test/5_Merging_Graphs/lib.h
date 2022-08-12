// I'm declaring this _extern "C"_ to avoid name mangling, making it easier to
// specify a symbol name for dlsym() later

extern "C" int myFunction();
