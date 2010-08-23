#ifndef ARG_HANDLER_H
#define ARG_HANDLER_H

/*
 Arg handling macros/functions.
 The following macros and embedded functions make for a nice tight declaration
 of command line arguments inside main() that parse those arguments.
 Accepts one or two dashes, =value or value in the next arg.
	int argi=1;
	while (argi < argc) {
		IntArg("name", &value);
		DoubleArg("name", &value);
		StringArg("name", &value);
		StringArgWithCopy("name", &value); // value gets strdup() of value
		StringArgWithCallback("name", callback, context);
		BoolArg("name", &value);
		handle other args;
	}
 */

// return { 0: not claimed, 1: claimed, -1: error }
// Templatize just to accept either {const char,char}**
template<class T>
static int _IntArg(int argc, T** argv, int* argi,
			const char* optname, int* value) {
	T* arg = argv[*argi];
	// skip "-" or "--"
	if (*arg == '-') {
		arg = arg + 1;
	}
	if (*arg == '-') {
		arg = arg + 1;
	}
	while (*optname != '\0') {
		if (*optname != *arg) {
			return 0;
		}
		optname++;
		arg++;
	}
	T* valueStr;
	if (*arg == '=') {
		// parse the rest of this arg as the value
		valueStr = arg + 1;
	} else if (*arg == '\0') {
		*argi = *argi + 1;
		valueStr = argv[*argi];
	} else {
		return 0;
	}
	char* endp;
	long tv = strtol(valueStr, &endp, 10);
	if ((endp == NULL) || (endp == valueStr) || (*endp != '\0')) {
		fprintf(stderr, "for arg \"%s\" failed to parse int value \"%s\"\n",
				optname, valueStr);
		return -1;
	}
	*value = tv;
	return 1;
}
#define IntArg(optname, value) switch(_IntArg(argc, argv, &argi, (optname), (value))) { \
case -1: fprintf(stderr, "bogus argv[%d] \"%s\"\n", argi, argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;}

// return { 0: not claimed, 1: claimed, -1: error }
// Templatize just to accept either {const char,char}**
template<class T>
static int _DoubleArg(int argc, T** argv, int* argi,
			   const char* optname, double* value) {
	T* arg = argv[*argi];
	// skip "-" or "--"
	if (*arg == '-') {
		arg = arg + 1;
	}
	if (*arg == '-') {
		arg = arg + 1;
	}
	while (*optname != '\0') {
		if (*optname != *arg) {
			return 0;
		}
		optname++;
		arg++;
	}
	T* valueStr;
	if (*arg == '=') {
		// parse the rest of this arg as the value
		valueStr = arg + 1;
	} else if (*arg == '\0') {
		*argi = *argi + 1;
		valueStr = argv[*argi];
	} else {
		return 0;
	}
	char* endp;
	double tv = strtod(valueStr, &endp);
	if ((endp == NULL) || (endp == valueStr) || (*endp != '\0')) {
		fprintf(stderr, "for arg \"%s\" failed to parse int value \"%s\"\n",
				optname, valueStr);
		return -1;
	}
	*value = tv;
	return 1;
}
#define DoubleArg(optname, value) switch(_DoubleArg(argc, argv, &argi, (optname), (value))) { \
case -1: fprintf(stderr, "bogus argv[%d] \"%s\"\n", argi, argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;}

// return { 0: not claimed, 1: claimed, -1: error }
// Does not copy argv, but sets value to point into it.
template<class A, class B>
static int _StringArg(int argc, A** argv, int* argi,
					  const char* optname, B** value) {
	A* arg = argv[*argi];
	// skip "-" or "--"
	if (*arg == '-') {
		arg = arg + 1;
	}
	if (*arg == '-') {
		arg = arg + 1;
	}
	while (*optname != '\0') {
		if (*optname != *arg) {
			return 0;
		}
		optname++;
		arg++;
	}
	if (*arg == '=') {
		// parse the rest of this arg as the value
		*value = arg + 1;
	} else if (*arg == '\0') {
		*argi = *argi + 1;
		*value = argv[*argi];
	} else {
		return 0;
	}
	return 1;
}

#define StringArg(optname, value) switch(_StringArg(argc, argv, &argi, (optname), (value))) { \
case -1: fprintf(stderr, "bogus argv[%d] \"%s\"\n", argi, argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;}

#define StringArgWithCopy(optname, value) switch(_StringArg(argc, argv, &argi, (optname), (value))) { \
case -1: fprintf(stderr, "bogus argv[%d] \"%s\"\n", argi, argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; (*(value)) = strdup(*(value)); continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;}

// Calls: callback(context, string value);
#define StringArgWithCallback(optname, callback, context) {const char* __cb_str_value = NULL; switch(_StringArg(argc, argv, &argi, (optname), &__cb_str_value)) { \
case -1: fprintf(stderr, "bogus argv[%d] \"%s\"\n", argi, argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; callback(context, __cb_str_value); continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;} \
}

// return { 0: not claimed, 1: claimed, -1: error }
// --name or --name=true sets true, --name=false sets false.
// ["--name", "false"] is _not_ accepted.
// Templatize just to accept either {const char,char}**
template<class T>
static inline int _BoolArg(int argc, T** argv, int* argi,
					  const char* optname, bool* value) {
	T* arg = argv[*argi];
	// skip "-" or "--"
	if (*arg == '-') {
		arg = arg + 1;
	}
	if (*arg == '-') {
		arg = arg + 1;
	}
	while (*optname != '\0') {
		if (*optname != *arg) {
			return 0;
		}
		optname++;
		arg++;
	}
	T* valueStr;
	if (*arg == '=') {
		// parse the rest of this arg as the value
		valueStr = arg + 1;
	} else if (*arg == '\0') {
		*value = true;
		return 1;
	} else {
		return 0;
	}
	if (!strcasecmp("true", valueStr)) {
		*value = true;
	} else if (!strcasecmp("false", valueStr)) {
		*value = false;
	} else {
		fprintf(stderr, "\"%s\" is not true or false\n", argv[*argi]);
		return -1;
	}
	return 1;
}
#define BoolArg(optname, value) switch(_BoolArg(argc, argv, &argi, (optname), (value))) { \
case -1: fprintf(stderr, "bogus arg \"%s\"\n", argv[argi]); exit(1); break; \
case 0: break; \
case 1: argi++; continue; \
default: fprintf(stderr, "ICE %d\n", __LINE__); exit(1); return 1;}


#endif /* ARG_HANDLER_H */
