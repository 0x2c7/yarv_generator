#include "ruby/ruby.h"
#include "ruby/st.h"
#include "vm_core.h"
#include "eval_intern.h"
#include "ruby/encoding.h"
#include "internal.h"
#include "iseq.h"
#include "node.h"
#include "insns.inc"
#include "insns_info.inc"

VALUE yarv_builder_build_from_source(VALUE self, VALUE src);
VALUE yarv_builder_build_yarv_tree(rb_iseq_t *iseq);
VALUE yarv_builder_insn_type(rb_iseq_t *iseq);
VALUE yarv_builder_local_table(rb_iseq_t *iseq);
VALUE yarv_builder_instructions(rb_iseq_t *iseq);
VALUE yarv_builder_call_info(VALUE *seq);
VALUE obj_resurrect(VALUE obj);
VALUE register_label(struct st_table *table, unsigned long idx);
int cdhash_each(VALUE key, VALUE value, VALUE ary);

VALUE
yarv_builder_build_from_source(VALUE self, VALUE src)
{
  rb_iseq_t *iseq = rb_iseq_compile_with_option(src, rb_str_new2("<compiled>"), Qnil, INT2FIX(1), 0, Qnil);
  return yarv_builder_build_yarv_tree(iseq);
}

VALUE
yarv_builder_build_yarv_tree(rb_iseq_t *iseq)
{
  VALUE rb_cYarvIseq = rb_path2class("YarvGenerator::Iseq");
  VALUE iseq_object = rb_funcall(rb_cYarvIseq, rb_intern("new"), 0);

  VALUE type = yarv_builder_insn_type(iseq);
  rb_funcall(iseq_object, rb_intern("type="), 1, type);
  VALUE local_table = yarv_builder_local_table(iseq);
  rb_funcall(iseq_object, rb_intern("local_table="), 1, local_table);
  VALUE instructions = yarv_builder_instructions(iseq);
  rb_funcall(iseq_object, rb_intern("instructions="), 1, instructions);

  return iseq_object;
}


VALUE
yarv_builder_insn_type(rb_iseq_t *iseq)
{
  switch(iseq->body->type) {
    case ISEQ_TYPE_TOP:
      return ID2SYM(rb_intern("top"));
      break;
    case ISEQ_TYPE_MAIN:
      return ID2SYM(rb_intern("main"));
      break;
    case ISEQ_TYPE_EVAL:
      return ID2SYM(rb_intern("eval"));
      break;
    case ISEQ_TYPE_METHOD:
      return ID2SYM(rb_intern("method"));
      break;
    case ISEQ_TYPE_BLOCK:
      return ID2SYM(rb_intern("block"));
      break;
    case ISEQ_TYPE_CLASS:
      return ID2SYM(rb_intern("class"));
      break;
    case ISEQ_TYPE_RESCUE:
      return ID2SYM(rb_intern("rescue"));
      break;
    case ISEQ_TYPE_ENSURE:
      return ID2SYM(rb_intern("ensure"));
      break;
    case ISEQ_TYPE_DEFINED_GUARD:
      return ID2SYM(rb_intern("defined_guard"));
      break;
    default: rb_bug("iseq type %d not found", iseq->body->type);
  };
}

VALUE
yarv_builder_local_table(rb_iseq_t *iseq)
{
  VALUE local_table = rb_ary_new();
  for (unsigned int i = 0; i < iseq->body->local_table_size; i++) {
    ID var_id = iseq->body->local_table[i];
    if (var_id) {
      rb_ary_push(local_table, ID2SYM(var_id));
    } else {
      rb_ary_push(local_table, ID2SYM(rb_intern("arg_rest")));
    }
  }
  return local_table;
}


VALUE
yarv_builder_instructions(rb_iseq_t *iseq)
{
  VALUE instructions = rb_ary_new();
  VALUE rb_cYarvInstruction = rb_path2class("YarvGenerator::Instruction");

  VALUE *iseq_original = rb_iseq_original_iseq((rb_iseq_t *)iseq);
  struct st_table *labels_table = st_init_numtable();

  for (VALUE *seq = iseq_original; seq < iseq_original + iseq->body->iseq_size; ) {
    VALUE insn = *seq++;
    int len = insn_len_info[(int)insn];
    VALUE operands = rb_ary_new2(len);

    VALUE *nseq = seq + len;

    for (int j = 0; j < len - 1; j++, seq++) {
      VALUE operand = Qnil;
      switch (insn_op_type(insn, j)) {
        case TS_LINDEX:
        case TS_NUM:
          operand = INT2FIX(*seq);
          break;
        case TS_VALUE:
          operand = obj_resurrect(*seq);
          break;
        case TS_CALLCACHE:
          operand = Qfalse;
          break;
        case TS_ID:
          operand = ID2SYM(*seq);
          break;
        case TS_FUNCPTR:
#if SIZEOF_VALUE <= SIZEOF_LONG
          operand = LONG2NUM((SIGNED_VALUE)*seq);
#else
          operand = LL2NUM((SIGNED_VALUE)*seq);
#endif
          break;
        case TS_GENTRY:
          {
            struct rb_global_entry *entry = (struct rb_global_entry *)*seq;
            operand = ID2SYM(entry->id);
          }
          break;
        case TS_OFFSET:
          {
            unsigned long idx = nseq - iseq_original + *seq;
            operand = register_label(labels_table, idx);
          }
          break;
        case TS_IC:
          {
            union iseq_inline_storage_entry *is = (union iseq_inline_storage_entry *)*seq;
            operand = INT2FIX(is - iseq->body->is_entries);
          }
          break;
        case TS_ISEQ:
          {
            const rb_iseq_t *iseq = (rb_iseq_t *)*seq;
            if (iseq) {
              operand = yarv_builder_build_yarv_tree((rb_iseq_t *)rb_iseq_check(iseq));
            }
            else {
              operand = Qnil;
            }
          }
          break;
        case TS_CALLINFO:
          operand = yarv_builder_call_info(seq);
          break;
        case TS_CDHASH:
          {
            VALUE hash = *seq;
            operand = rb_ary_new();
            rb_hash_foreach(hash, cdhash_each, operand);

            for (int i=0; i<RARRAY_LEN(operand); i+=2) {
              VALUE pos = FIX2INT(rb_ary_entry(operand, i+1));
              unsigned long idx = nseq - iseq_original + pos;

              rb_ary_store(operand, i+1, register_label(labels_table, idx));
            }
          }
          break;
        default:
          rb_bug("unknown operand: %c", insn_op_type(insn, j));
          break;
      }
      rb_ary_push(operands, operand);
    }

    VALUE insn_object = rb_funcall(rb_cYarvInstruction, rb_intern("new"), 0);
    rb_funcall(insn_object, rb_intern("name="), 1, rb_str_new2(insn_name(insn)));
    rb_funcall(insn_object, rb_intern("operands="), 1, operands);
    rb_ary_push(instructions, insn_object);
  }
  return instructions;
}

VALUE
yarv_builder_call_info(VALUE *seq)
{
  VALUE rb_cYarvCallInfo = rb_path2class("YarvGenerator::CallInfo");
  struct rb_call_info *ci = (struct rb_call_info *)*seq;

  VALUE ci_object = rb_funcall(rb_cYarvCallInfo, rb_intern("new"), 0);
  rb_funcall(ci_object, rb_intern("mid="), 1, ci->mid ? ID2SYM(ci->mid) : Qnil);
  rb_funcall(ci_object, rb_intern("flag="), 1, UINT2NUM(ci->flag));
  rb_funcall(ci_object, rb_intern("orig_argc="), 1, INT2FIX(ci->orig_argc));

  if (ci->flag & VM_CALL_KWARG) {
    struct rb_call_info_with_kwarg *ci_kw = (struct rb_call_info_with_kwarg *)ci;
    VALUE kw = rb_ary_new2(ci_kw->kw_arg->keyword_len);
    int len = ci->orig_argc - ci_kw->kw_arg->keyword_len;
    for (int i = 0; i < len; i++) {
      rb_ary_push(kw, ci_kw->kw_arg->keywords[i]);
    }
    rb_funcall(ci_object, rb_intern("kw_arg="), 1, kw);
  }
  return ci_object;

}

// Private method copied from iseq.c
inline VALUE
obj_resurrect(VALUE obj)
{
  if (!SPECIAL_CONST_P(obj) && !RBASIC(obj)->klass) {
    switch (BUILTIN_TYPE(obj)) {
      case T_STRING:
        obj = rb_str_resurrect(obj);
        break;
      case T_ARRAY:
        obj = rb_ary_resurrect(obj);
        break;
    }
  }
  return obj;
}

// Private method copied from iseq.c
VALUE
register_label(struct st_table *table, unsigned long idx)
{
  VALUE sym = rb_str_intern(rb_sprintf("label_%lu", idx));
  st_insert(table, idx, sym);
  return sym;
}

// Private method copied from iseq.c
int
cdhash_each(VALUE key, VALUE value, VALUE ary)
{
  rb_ary_push(ary, obj_resurrect(key));
  rb_ary_push(ary, value);
  return ST_CONTINUE;
}

void Init_yarv_generator() {
  VALUE rb_cYarvGenerator = rb_path2class("YarvGenerator");
  VALUE rb_cYarvBuilder = rb_define_class_under(rb_cYarvGenerator, "Builder", rb_cObject);
  rb_define_method(rb_cYarvBuilder, "build_from_source", &yarv_builder_build_from_source, 1);
}
