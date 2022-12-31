#pragma once

#define DSCM_ARG1 "1"
#define DSCM_ARG2 "1"
#define DSCM_ARG3 "1"
#define DSCM_ARG4 "1"

#define DSCM_ASSERT(PRED, MSG, ...)					\
	if (!PRED) scm_misc_error(					\
		"", MSG, scm_list_n(__VA_ARGS__, SCM_UNDEFINED))

#define DSCM_ASSERT_TYPE(PRED, VALUE, SUBR, ARG, TYPE)			\
	DSCM_ASSERT(PRED, SUBR ": Wrong type argument in position " ARG \
		    " (expected " TYPE "): ~a", VALUE)

enum { DSCM_CALL_ARRANGE, DSCM_CALL_ACTION };
typedef struct {
	SCM proc;
	void *args;
} dscm_call_data_t;

typedef struct {
	char *exp;
	struct wl_resource *resource;
	void (*callback)(struct wl_resource*, char*, uint32_t);
} dscm_eval_call_data_t;

typedef void(*dscm_reloader_t)();
typedef void(*dscm_setter_t)(void*, SCM);

static inline int
dscm_is_callback(SCM cb)
{
	return scm_is_symbol(cb) || scm_procedure_p(cb) == SCM_BOOL_T;
}

static inline SCM
dscm_assoc_ref(SCM alist, const char* symbol)
{
	return scm_assoc_ref(alist, scm_string_to_symbol(scm_from_locale_string(symbol)));
}

static inline char*
dscm_assoc_ref_string(SCM alist, const char *symbol)
{
	SCM value = dscm_assoc_ref(alist, symbol);
	if (scm_is_string(value))
		return scm_to_locale_string(value);
	return NULL;
}

static inline scm_t_bits *
dscm_get_pointer(SCM action)
{
	SCM actionref = scm_primitive_eval(action);
	DSCM_ASSERT((scm_procedure_p(actionref) == SCM_BOOL_T),
		    "Invalid action callback: ~s", action);
	scm_gc_protect_object(actionref);
	return SCM_UNPACK_POINTER(actionref);
}

static inline unsigned int
dscm_get_tag(SCM tag, unsigned int tags)
{
	unsigned int target_tag = scm_to_unsigned_integer(tag, 1, tags) - 1;
	return (1 << (target_tag));
}

static inline void
dscm_parse_binding_sequence(Binding *b, char *sequence)
{
	char *token, *next, *key, *ptr;
	if ((key = strpbrk(sequence, "<"))) {
		SCM keycode = scm_hash_ref(
			keycodes, scm_from_locale_string(key), SCM_UNDEFINED);
		DSCM_ASSERT((!scm_is_false(keycode)),
			    "Invalid keysym in bind sequence: ~s",
			    scm_from_locale_string(sequence));
		b->key = (xkb_keycode_t)scm_to_uint32(keycode);
	} else if ((key = strpbrk(sequence, "["))) {
		for (ptr = &key[1]; !(*ptr == '\0' || *ptr == ']'); ptr++)
			DSCM_ASSERT(isdigit(*ptr),
				    "Invalid keycode in bind sequence: ~s",
				    scm_from_locale_string(sequence));
		DSCM_ASSERT((*ptr == ']'), "Trailing ']' in bind sequence: ~s",
			    scm_from_locale_string(sequence));
		*ptr = '\0';
		b->key = atoi(&key[1]);
	}

	/* Replace < or [ with NULL to make sure that it is not included
	   when parsing the modifiers. */
	if (key != NULL)
		key[0] = '\0';

	b->mod = 0;
	token = strtok_r(sequence, "-", &sequence);
	while (token) {
		if ((next = strtok_r(sequence, "-", &sequence)) || key != NULL) {
			if (!strcmp(token, "C"))
				b->mod |= WLR_MODIFIER_CTRL;
			else if (!strcmp(token, "M"))
				b->mod |= WLR_MODIFIER_ALT;
			else if (!strcmp(token, "S"))
				b->mod |= WLR_MODIFIER_SHIFT;
			else if (!strcmp(token, "s"))
				b->mod |= WLR_MODIFIER_LOGO; // Super (Mod4)
			else
				DSCM_ASSERT(0,
					    "Invalid modifier '~s' in bind sequence: ~s",
					    scm_from_locale_string(token),
					    scm_from_locale_string(sequence));
		} else {
			SCM keycode = scm_hash_ref(
				keycodes, scm_from_locale_string(token), SCM_UNDEFINED);
			DSCM_ASSERT((!scm_is_false(keycode)),
				    "Invalid keysym in bind sequence: ~s",
				    scm_from_locale_string(sequence));
			b->key = (xkb_keycode_t)scm_to_uint32(keycode);
		}
		token = next;
	}
}

static inline SCM
dscm_call_eval(void *data)
{
	dscm_eval_call_data_t *call_data = data;
	SCM eval = scm_c_eval_string(call_data->exp);
	SCM evalstr = scm_object_to_string(eval, SCM_UNDEFINED);
	char *result = scm_to_locale_string(evalstr);
	(*(call_data->callback))(call_data->resource, result,
				 DSCM_V1_EVAL_STATUS_SUCCESS);
	free(call_data->exp);
	free(call_data);
	free(result);
	return SCM_BOOL_T;
}

static inline SCM
dscm_call_thread_handler(void *data, SCM key, SCM args)
{
	dscm_eval_call_data_t *call_data = data;
	SCM error = scm_apply_2(
		scm_c_public_ref("guile", "format"),
		SCM_BOOL_F,
		scm_cadr(args),
		scm_caddr(args));
	char *errorstr = scm_to_locale_string(error);
	fprintf(stderr, "dscm: error in eval of %s\n=> %s\n",
		call_data->exp, errorstr);
	(*(call_data->callback))(call_data->resource, errorstr,
				 DSCM_V1_EVAL_STATUS_ERROR);
	free(errorstr);
	free(call_data->exp);
	free(call_data);
	return SCM_BOOL_T;
}

static inline void*
dscm_call_action(void *data)
{
	return scm_call_0(((dscm_call_data_t*)data)->proc);
}

static inline void*
dscm_call_arrange(void *data)
{
	dscm_call_data_t *proc_data = (dscm_call_data_t*)data;
	SCM mon = scm_from_pointer(proc_data->args, NULL);
	return scm_call_1(proc_data->proc, mon);
}

static inline void
dscm_safe_call(unsigned int type, scm_t_bits *proc_ptr, void *data)
{
	if (proc_ptr == NULL)
		die("dscm: could not call proc that is NULL");
	SCM proc = SCM_PACK_POINTER(proc_ptr);
	void*(*func)(void*) = type == DSCM_CALL_ARRANGE ?
		&dscm_call_arrange : &dscm_call_action;
	scm_c_with_continuation_barrier(
		func, &((dscm_call_data_t){.proc = proc, .args = data}));
}

static inline void
dscm_thread_eval(struct wl_resource *resource,
		void (*callback)(struct wl_resource*, char*, uint32_t),
		char *exp)
{
	dscm_eval_call_data_t *proc_data = ecalloc(1, sizeof(dscm_eval_call_data_t));
	proc_data->exp = exp;
	proc_data->resource = resource;
	proc_data->callback = callback;
	scm_spawn_thread(dscm_call_eval, proc_data,
			 dscm_call_thread_handler, proc_data);
}