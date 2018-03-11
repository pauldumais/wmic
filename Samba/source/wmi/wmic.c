/*
   WMI Sample client
   Copyright (C) 2006 Andrzej Hajda <andrzej.hajda@wp.pl>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "lib/cmdline/popt_common.h"
#include "auth/credentials/credentials.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ndr_oxidresolver.h"
#include "librpc/gen_ndr/ndr_oxidresolver_c.h"
#include "librpc/gen_ndr/dcom.h"
#include "librpc/gen_ndr/ndr_dcom.h"
#include "librpc/gen_ndr/ndr_dcom_c.h"
#include "librpc/gen_ndr/ndr_remact_c.h"
#include "librpc/gen_ndr/ndr_epmapper_c.h"
#include "librpc/gen_ndr/com_dcom.h"
#include "librpc/rpc/dcerpc_table.h"

#include "lib/com/dcom/dcom.h"
#include "lib/com/proto.h"
#include "lib/com/dcom/proto.h"

#include "wmi/wmi.h"

#include "wmi/proto.h"

struct program_args {
    char *hostname;
    char *query;
    char *ns;
    char *delim;
    char *path;
    char *method;	
};

int OPT_DELETE = 0;

static void parse_args(int argc, char *argv[], struct program_args *pmyargs)
{
    poptContext pc;
    int opt, i;

    int argc_new;
    char **argv_new;


    struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{"namespace", 0, POPT_ARG_STRING, &pmyargs->ns, 0, "WMI namespace, default to root\\cimv2", 0},
		{"delimiter", 0, POPT_ARG_STRING, &pmyargs->delim, 0, "delimiter to use when querying multiple values, default to '|'", 0},
		{"host", 0, POPT_ARG_STRING, &pmyargs->hostname, 0, "WMI Host", 0},
		{"query", 0, POPT_ARG_STRING, &pmyargs->query, 0, "WMI Query", 0},
		{"path", 0, POPT_ARG_STRING, &pmyargs->path, 0, "WMI Object Path", 0},
		{"call", 0, POPT_ARG_STRING, &pmyargs->method, 0, "WMI Object Call Method", 0},
		{"delete", 0, POPT_ARG_NONE, &OPT_DELETE, 0, "WMI Object Delete Instance"},
		POPT_TABLEEND
    };

    pc = poptGetContext("wmi", argc, (const char **) argv,
	        long_options, POPT_CONTEXT_KEEP_FIRST);

    poptSetOtherOptionHelp(pc, 
			"\n\nExamples:\
			\n    wmic -U [domain/]adminuser%password --host hostname --query \"select * from Win32_ComputerSystem\"\
			\n    wmic -U [domain/]adminuser%password --host hostname --path \"Win32_OperatingSystem\" --call Reboot arg1 arg2\
			\n    wmic -U [domain/]adminuser%password --host hostname --path \"Win32_PrintJob.Name='Job Name'\" --delete"
			);

    while ((opt = poptGetNextOpt(pc)) != -1) {
		poptPrintUsage(pc, stdout, 0);
		poptFreeContext(pc);
		exit(1);
    }

    argv_new = discard_const_p(char *, poptGetArgs(pc));

    argc_new = argc;
    for (i = 0; i < argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
    }

	if (pmyargs->hostname == NULL) {
		poptPrintUsage(pc, stdout, 0);
		poptFreeContext(pc);
		exit(1);
	}

	if (pmyargs->query==NULL && pmyargs->method==NULL && !OPT_DELETE) {
		// one of query, method or delete is required
		poptPrintUsage(pc, stdout, 0);
		poptFreeContext(pc);
		exit(1);
	}

	if ((pmyargs->method!=NULL || OPT_DELETE) && pmyargs->path==NULL) {
		// path is required
		poptPrintUsage(pc, stdout, 0);
		poptFreeContext(pc);
		exit(1);
	}

/*
    if (argc_new != 3 || strncmp(argv_new[1], "//", 2) != 0) {
		poptPrintUsage(pc, stdout, 0);
		poptFreeContext(pc);
		exit(1);
    }*/

    poptFreeContext(pc);
}

#define WERR_CHECK(msg) if (!W_ERROR_IS_OK(result)) { \
			    DEBUG(0, ("ERROR: %s\n", msg)); \
			    goto error; \
			} else { \
			    DEBUG(1, ("OK   : %s\n", msg)); \
			}

#define RETURN_CVAR_ARRAY_STR(fmt, arr) {\
        uint32_t i;\
	char *r;\
\
        if (!arr) {\
                return talloc_strdup(mem_ctx, "NULL");\
        }\
	r = talloc_strdup(mem_ctx, "(");\
        for (i = 0; i < arr->count; ++i) {\
		r = talloc_asprintf_append(r, fmt "%s", arr->item[i], (i+1 == arr->count)?"":",");\
        }\
        return talloc_asprintf_append(r, ")");\
}

char *string_CIMVAR(TALLOC_CTX *mem_ctx, union CIMVAR *v, enum CIMTYPE_ENUMERATION cimtype)
{
	switch (cimtype) {
        case CIM_SINT8: return talloc_asprintf(mem_ctx, "%d", v->v_sint8);
        case CIM_UINT8: return talloc_asprintf(mem_ctx, "%u", v->v_uint8);
        case CIM_SINT16: return talloc_asprintf(mem_ctx, "%d", v->v_sint16);
        case CIM_UINT16: return talloc_asprintf(mem_ctx, "%u", v->v_uint16);
        case CIM_SINT32: return talloc_asprintf(mem_ctx, "%d", v->v_sint32);
        case CIM_UINT32: return talloc_asprintf(mem_ctx, "%u", v->v_uint32);
        case CIM_SINT64: return talloc_asprintf(mem_ctx, "%lld", v->v_sint64);
        case CIM_UINT64: return talloc_asprintf(mem_ctx, "%llu", v->v_sint64);
        case CIM_REAL32: return talloc_asprintf(mem_ctx, "%f", (double)v->v_uint32);
        case CIM_REAL64: return talloc_asprintf(mem_ctx, "%f", (double)v->v_uint64);
        case CIM_BOOLEAN: return talloc_asprintf(mem_ctx, "%s", v->v_boolean?"True":"False");
        case CIM_STRING:
        case CIM_DATETIME:
        case CIM_REFERENCE: return talloc_asprintf(mem_ctx, "%s", v->v_string);
        case CIM_CHAR16: return talloc_asprintf(mem_ctx, "Unsupported");
        case CIM_OBJECT: return talloc_asprintf(mem_ctx, "Unsupported");
        case CIM_ARR_SINT8: RETURN_CVAR_ARRAY_STR("%d", v->a_sint8);
        case CIM_ARR_UINT8: RETURN_CVAR_ARRAY_STR("%u", v->a_uint8);
        case CIM_ARR_SINT16: RETURN_CVAR_ARRAY_STR("%d", v->a_sint16);
        case CIM_ARR_UINT16: RETURN_CVAR_ARRAY_STR("%u", v->a_uint16);
        case CIM_ARR_SINT32: RETURN_CVAR_ARRAY_STR("%d", v->a_sint32);
        case CIM_ARR_UINT32: RETURN_CVAR_ARRAY_STR("%u", v->a_uint32);
        case CIM_ARR_SINT64: RETURN_CVAR_ARRAY_STR("%lld", v->a_sint64);
        case CIM_ARR_UINT64: RETURN_CVAR_ARRAY_STR("%llu", v->a_uint64);
        case CIM_ARR_REAL32: RETURN_CVAR_ARRAY_STR("%f", v->a_real32);
        case CIM_ARR_REAL64: RETURN_CVAR_ARRAY_STR("%f", v->a_real64);
        case CIM_ARR_BOOLEAN: RETURN_CVAR_ARRAY_STR("%d", v->a_boolean);
        case CIM_ARR_STRING: RETURN_CVAR_ARRAY_STR("%s", v->a_string);
        case CIM_ARR_DATETIME: RETURN_CVAR_ARRAY_STR("%s", v->a_datetime);
        case CIM_ARR_REFERENCE: RETURN_CVAR_ARRAY_STR("%s", v->a_reference);
	default: return talloc_asprintf(mem_ctx, "Unsupported");
	}
}

#undef RETURN_CVAR_ARRAY_STR

int main(int argc, char **argv)
{
	struct program_args args = {};
	uint32_t cnt = 5, ret;
	char *class_name = NULL;
	char *key_name = NULL;
	char *key_value = NULL;
	WERROR result;
	NTSTATUS status;
	struct IWbemServices *pWS = NULL;

        parse_args(argc, argv, &args);
	
	/* apply default values if not given by user*/
	if (!args.ns) args.ns = "root\\cimv2";
	if (!args.delim) args.delim = "|";

	dcerpc_init();
	dcerpc_table_init();

	dcom_proxy_IUnknown_init();
	dcom_proxy_IWbemLevel1Login_init();
	dcom_proxy_IWbemServices_init();
	dcom_proxy_IEnumWbemClassObject_init();
	dcom_proxy_IRemUnknown_init();
	dcom_proxy_IWbemFetchSmartEnum_init();
	dcom_proxy_IWbemWCOSmartEnum_init();
	dcom_proxy_IWbemClassObject_init();

	struct com_context *ctx = NULL;
	com_init_ctx(&ctx, NULL);
	dcom_client_init(ctx, cmdline_credentials);

	result = WBEM_ConnectServer(ctx, args.hostname, args.ns, 0, 0, 0, 0, 0, 0, &pWS);
	WERR_CHECK("Login to remote object.");

	struct IWbemClassObject *wco = NULL;
	struct IWbemClassObject *inc, *outc, *in;
	struct IWbemClassObject *out = NULL;

	if (OPT_DELETE) {
		result = IWbemServices_DeleteInstance(pWS, ctx, args.path, 0, NULL, NULL);
		WERR_CHECK("WMI delete.");

	}
	else if (args.method != NULL) {

		union CIMVAR v;

		// TBD Add args
/*		printf("get object\n");

		result = IWbemServices_GetObject(pWS, ctx, "Win32_Service", WBEM_FLAG_RETURN_WBEM_COMPLETE, NULL, &wco, NULL);
		WERR_CHECK("GetObject.");

		printf("%s\n", args.path);

		printf("%s\n", args.method);

		printf("get method\n");

		result = IWbemClassObject_GetMethod(wco, ctx, args.method, 0, &inc, &outc);
		WERR_CHECK("IWbemClassObject_GetMethod.");

		printf("spawn\n");

		result = IWbemClassObject_SpawnInstance(inc, ctx, 0, &in);
		WERR_CHECK("IWbemClassObject_SpawnInstance.");

		printf("put arg\n");

		v.v_string = "Automatic";
		result = IWbemClassObject_Put(in, ctx, "StartMode", 0, &v, 0);
		WERR_CHECK("IWbemClassObject_Put.");

		printf("exec\n");
*/
		result = IWbemServices_ExecMethod(pWS, ctx, args.path, args.method, 0, NULL, in, &out, NULL);
		WERR_CHECK("WMI method execute.");

		result = WbemClassObject_Get(out->object_data, ctx, "ReturnValue", 0, &v, 0, 0);
		WERR_CHECK("ReturnValue.");
//		*ret_code = v.v_uint32;
		printf("%i", v.v_uint32);	
/*
			result = IWbemServices_GetObject(pWS, ctx, "Win32_Process", WBEM_FLAG_RETURN_WBEM_COMPLETE, NULL, &wco, NULL);
	WERR_CHECK("GetObject.");

	result = IWbemClassObject_GetMethod(wco, ctx, "Create", 0, &inc, &outc);
	WERR_CHECK("IWbemClassObject_GetMethod.");

	result = IWbemClassObject_SpawnInstance(inc, ctx, 0, &in);
	WERR_CHECK("IWbemClassObject_SpawnInstance.");

	union CIMVAR v;
	v.v_string = "notepad.exe";
	result = IWbemClassObject_Put(in, ctx, "CommandLine", 0, &v, 0);
	WERR_CHECK("IWbemClassObject_Put(CommandLine).");

	result = IWbemServices_ExecMethod(pWS, ctx, "Win32_Process", "Create", 0, NULL, in, &out, NULL);
	WERR_CHECK("IWbemServices_ExecMethod.");
	*/
	}
	else {

		struct IEnumWbemClassObject *pEnum = NULL;
		result = IWbemServices_ExecQuery(pWS, ctx, "WQL", args.query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_ENSURE_LOCATABLE, NULL, &pEnum);
		WERR_CHECK("WMI query execute.");

		IEnumWbemClassObject_Reset(pEnum, ctx);
		WERR_CHECK("Reset result of WMI query.");

		do {
			uint32_t i, j, k, key_index;
			struct WbemClassObject *co[cnt];

			result = IEnumWbemClassObject_SmartNext(pEnum, ctx, 0xFFFFFFFF, cnt, co, &ret);
			if (!W_ERROR_EQUAL(result, WERR_BADFUNC)) {
				WERR_CHECK("Retrieve result data.");
			} else {
				DEBUG(1, ("OK   : Retrieved less objects than requested (it is normal).\n"));
			}
			if (!ret) break;

			for (i = 0; i < ret; ++i) {
				if (!class_name || strcmp(co[i]->obj_class->__CLASS, class_name)) {
					if (class_name) talloc_free(class_name);
					class_name = talloc_strdup(ctx, co[i]->obj_class->__CLASS);
	//				printf("CLASS: %s\n", class_name);
					for (j = 0; j < co[i]->obj_class->__PROPERTY_COUNT; ++j) {
						printf("%s%s", j?args.delim:"", co[i]->obj_class->properties[j].name);

						for (k = 0; k < co[i]->obj_class->properties[j].desc->qualifiers.count; ++k) {
							if (!strcmp(co[i]->obj_class->properties[j].desc->qualifiers.item[k]->name, "key")) {
								printf("IS KEY");
								if (key_name) talloc_free(key_name);
								key_name = talloc_strdup(ctx, co[i]->obj_class->properties[j].name);
								key_index = j;
							}
						}
					}
					printf("%s%s", args.delim,"ObjectClass");
					printf("%s%s", args.delim,"ObjectPath");
					printf("\n");
				}

				for (j = 0; j < co[i]->obj_class->__PROPERTY_COUNT; ++j) {
					char *s;
					s = string_CIMVAR(ctx, &co[i]->instance->data[j], co[i]->obj_class->properties[j].desc->cimtype & CIM_TYPEMASK);
					printf("%s%s", j?args.delim:"", s);
					if (j==key_index) {
						key_value = s;
					}
				}

				printf("%s%s", args.delim,class_name);
				printf("%s%s.%s='%s'", args.delim,class_name, key_name, key_value);


				printf("\n");
			}
		} while (ret == cnt);
	}


	talloc_free(ctx);
	return 0;
error:
	status = werror_to_ntstatus(result);
	fprintf(stderr, "NTSTATUS: %s - %s\n", nt_errstr(status), get_friendly_nt_error_msg(status));
	talloc_free(ctx);
	return 1;
}
