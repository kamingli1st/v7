#include "internal.h"

V7_PRIVATE enum v7_err String_ctor(struct v7_c_func_arg *cfa) {
  struct v7_val *obj = cfa->called_as_constructor ? cfa->this_obj : v7_push_new_object(cfa->v7);
  struct v7_val *arg = cfa->args[0];
  struct v7 *v7 = cfa->v7;  // Needed for TRY() macro below

  // If argument is not a string, do type conversion
  if (cfa->num_args == 1 && !is_string(arg)) {
    TRY(toString(cfa->v7, arg));
    arg = v7_top_val(cfa->v7);
  }

  if (cfa->num_args == 1 && arg->type == V7_TYPE_STR) {
    v7_init_str(obj, arg->v.str.buf, arg->v.str.len, 1);
  } else {
    v7_init_str(obj, NULL, 0, 0);
  }

  v7_set_class(obj, V7_CLASS_STRING);
  return V7_OK;
}

V7_PRIVATE void Str_length(struct v7_val *this_obj, struct v7_val *result) {
  v7_init_num(result, this_obj->v.str.len);
}

static const char *StrAt(struct v7_c_func_arg *cfa) {
  if (cfa->num_args > 0 && cfa->args[0]->type == V7_TYPE_NUM &&
    cfa->this_obj->type == V7_TYPE_STR &&
    fabs(cfa->args[0]->v.num) < cfa->this_obj->v.str.len) {
    int idx = (int) cfa->args[0]->v.num;
    const char *p = cfa->this_obj->v.str.buf;
    return idx > 0 ? p + idx : p + cfa->this_obj->v.str.len - idx;
  }
  return NULL;
}

V7_PRIVATE enum v7_err Str_charCodeAt(struct v7_c_func_arg *cfa) {
  const char *p = StrAt(cfa);
  v7_push_number(cfa->v7, p == NULL ? NAN : p[0]);
  return V7_OK;
}

V7_PRIVATE enum v7_err Str_charAt(struct v7_c_func_arg *cfa) {
  const char *p = StrAt(cfa);
  v7_push_string(cfa->v7, p, p == NULL ? 0 : 1, 1);
  return V7_OK;
}

V7_PRIVATE enum v7_err Str_match(struct v7_c_func_arg *cfa) {
  struct v7_val *arg = cfa->args[0];
  struct v7 *v7 = cfa->v7;  // Needed for TRY() macro below
  struct Resub sub;
  struct v7_val *arr = NULL;
  int shift = 0;

  if(cfa->num_args > 0){
    if(!instanceof(arg, &s_constructors[V7_CLASS_REGEXP]) && !is_string(arg)){ // If argument is not a RegExp/string, do type conversion
      TRY(toString(v7, arg));
      arg = v7_top_val(v7);
    }
    TRY(regex_check_prog(arg));
    do{
      if(!re_exec(arg->v.str.prog, arg->fl, cfa->this_obj->v.str.buf + shift, &sub)){
        if(NULL == arr){
          arr = v7_push_new_object(v7);
          v7_set_class(arr, V7_CLASS_ARRAY);
        }
        shift = sub.sub[0].end - cfa->this_obj->v.str.buf;
        v7_append(v7, arr, v7_mkv(v7, V7_TYPE_STR, sub.sub[0].start, sub.sub[0].end - sub.sub[0].start, 1));
      }
    }while(arg->fl.re_g && shift < cfa->this_obj->v.str.len);
  }
  if(0 == shift) TRY(v7_make_and_push(v7, V7_TYPE_NULL));
  return V7_OK;
}

V7_PRIVATE enum v7_err Str_split(struct v7_c_func_arg *cfa) {
  struct v7_val *arg = cfa->args[0];
  struct v7 *v7 = cfa->v7;  // Needed for TRY() macro below
  struct Resub sub, sub1;
  struct v7_val *arr = v7_push_new_object(v7);
  int limit = 100000, elem = 0, shift = 0, i, len;

  v7_set_class(arr, V7_CLASS_ARRAY);
  if(cfa->num_args > 0){
    if(cfa->num_args > 1 && cfa->args[1]->type == V7_TYPE_NUM) limit = cfa->args[1]->v.num;
    if(!instanceof(arg, &s_constructors[V7_CLASS_REGEXP]) && !is_string(arg)){ // If argument is not a RegExp/string, do type conversion
      TRY(toString(v7, arg));
      arg = v7_top_val(v7);
    }
    TRY(regex_check_prog(arg));
    for(; elem < limit && shift < cfa->this_obj->v.str.len; elem++){
      if(re_exec(arg->v.str.prog, arg->fl, cfa->this_obj->v.str.buf + shift, &sub)) break;
      v7_append(v7, arr, v7_mkv(v7, V7_TYPE_STR, cfa->this_obj->v.str.buf + shift, sub.sub[0].start - cfa->this_obj->v.str.buf - shift, 1));
      for(i = 1; i < sub.subexpr_num; i++)
        v7_append(v7, arr, v7_mkv(v7, V7_TYPE_STR, sub.sub[i].start, sub.sub[i].end - sub.sub[i].start, 1));
      shift = sub.sub[0].end - cfa->this_obj->v.str.buf;
      sub1=sub;
    }
    len = cfa->this_obj->v.str.len - shift;
    if(shift > 0 && elem < limit && len > 0)
      v7_append(v7, arr, v7_mkv(v7, V7_TYPE_STR, sub1.sub[0].end, len, 1));
  }
  if(0 == shift) v7_append(v7, arr, v7_mkv(v7, V7_TYPE_STR, cfa->this_obj->v.str.buf, cfa->this_obj->v.str.len, 1));

  return V7_OK;
}

V7_PRIVATE enum v7_err Str_indexOf(struct v7_c_func_arg *cfa) {
  double index = -1.0;
  if (cfa->this_obj->type == V7_TYPE_STR &&
      cfa->num_args > 0 &&
      cfa->args[0]->type == V7_TYPE_STR) {
    int i = cfa->num_args > 1 && cfa->args[1]->type == V7_TYPE_NUM ?
    (int) cfa->args[1]->v.num : 0;
    const struct v7_string *a = &cfa->this_obj->v.str,
    *b = &cfa->args[0]->v.str;

    // Scan the string, advancing one byte at a time
    for (; i >= 0 && a->len >= b->len && i <= (int) (a->len - b->len); i++) {
      if (memcmp(a->buf + i, b->buf, b->len) == 0) {
        index = i;
        break;
      }
    }
  }
  v7_push_number(cfa->v7, index);
  return V7_OK;
}

V7_PRIVATE enum v7_err Str_substr(struct v7_c_func_arg *cfa) {
  struct v7_val *result = v7_push_string(cfa->v7, NULL, 0, 0);
  long start = 0;

  if (cfa->num_args > 0 && cfa->args[0]->type == V7_TYPE_NUM) {
    start = (long) cfa->args[0]->v.num;
  }
  if (start < 0) {
    start += (long) cfa->this_obj->v.str.len;
  }
  if (start >= 0 && start < (long) cfa->this_obj->v.str.len) {
    long n = cfa->this_obj->v.str.len - start;
    if (cfa->num_args > 1 && cfa->args[1]->type == V7_TYPE_NUM) {
      n = cfa->args[1]->v.num;
    }
    if (n > 0 && n <= ((long) cfa->this_obj->v.str.len - start)) {
      v7_init_str(result, cfa->this_obj->v.str.buf + start, n, 1);
    }
  }
  return V7_OK;
}

V7_PRIVATE enum v7_err Str_search(struct v7_c_func_arg *cfa) {
  struct v7_val *arg = cfa->args[0];
  struct v7 *v7 = cfa->v7;  // Needed for TRY() macro below
  struct Resub sub;
  int shift = -1;

  if(cfa->num_args > 0){
    if(!instanceof(arg, &s_constructors[V7_CLASS_REGEXP]) && !is_string(arg)){ // If argument is not a RegExp/string, do type conversion
      TRY(toString(v7, arg));
      arg = v7_top_val(v7);
    }
    TRY(regex_check_prog(arg));
    if(!re_exec(arg->v.str.prog, arg->fl, cfa->this_obj->v.str.buf, &sub)) shift = sub.sub[0].start - cfa->this_obj->v.str.buf;
  }
  v7_push_number(v7, (double)shift);
  return V7_OK;
}

V7_PRIVATE void init_string(void) {
  init_standard_constructor(V7_CLASS_STRING, String_ctor);
  SET_PROP_FUNC(s_prototypes[V7_CLASS_STRING], "length", Str_length);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "charCodeAt", Str_charCodeAt);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "charAt", Str_charAt);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "indexOf", Str_indexOf);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "substr", Str_substr);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "match", Str_match);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "split", Str_split);
  SET_METHOD(s_prototypes[V7_CLASS_STRING], "search", Str_search);

  SET_RO_PROP_V(s_global, "String", s_constructors[V7_CLASS_STRING]);
}
