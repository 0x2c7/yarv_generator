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
VALUE yarv_builder_instructions(rb_iseq_t *iseq, st_table *labels_table);
VALUE yarv_builder_params(rb_iseq_t *iseq, st_table *labels_table);
VALUE yarv_builder_catch_table(rb_iseq_t *iseq, st_table *labels_table);
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

  struct st_table *labels_table = st_init_numtable();

  VALUE type = yarv_builder_insn_type(iseq);
  rb_funcall(iseq_object, rb_intern("type="), 1, type);

  VALUE local_table = yarv_builder_local_table(iseq);
  rb_funcall(iseq_object, rb_intern("local_table="), 1, local_table);

  VALUE params = yarv_builder_params(iseq, labels_table);
  rb_funcall(iseq_object, rb_intern("params="), 1, params);

  VALUE catch_table = yarv_builder_catch_table(iseq, labels_table);
  rb_funcall(iseq_object, rb_intern("catch_table="), 1, catch_table);

  VALUE instructions = yarv_builder_instructions(iseq, labels_table);
  rb_funcall(iseq_object, rb_intern("instructions="), 1, instructions);

  rb_funcall(iseq_object, rb_intern("label="), 1, iseq->body->location.label);
  rb_funcall(iseq_object, rb_intern("path="), 1, iseq->body->location.path);
  rb_funcall(iseq_object, rb_intern("absolute_path="), 1, iseq->body->location.absolute_path);
  rb_funcall(iseq_object, rb_intern("first_lineno="), 1, iseq->body->location.first_lineno);
  rb_funcall(iseq_object, rb_intern("arg_size="), 1, INT2FIX(iseq->body->param.size));
  rb_funcall(iseq_object, rb_intern("local_size="), 1, INT2FIX(iseq->body->local_size));
  rb_funcall(iseq_object, rb_intern("stack_max="), 1, INT2FIX(iseq->body->stack_max));

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
      if (rb_id2str(var_id)) {
        rb_ary_push(local_table, ID2SYM(var_id));
      }
      else { /* Fix weird ID problem. This fix comes from iseq.c */
        rb_ary_push(local_table, ULONG2NUM(iseq->body->local_table_size-i+1));
      }
    } else {
      rb_ary_push(local_table, ID2SYM(rb_intern("arg_rest")));
    }
  }
  return local_table;
}


VALUE
yarv_builder_instructions(rb_iseq_t *iseq, st_table *labels_table)
{
  VALUE instructions = rb_ary_new();
  VALUE rb_cYarvIBuilder = rb_path2class("YarvGenerator::InstructionBuilder");
  VALUE instruction_builder = rb_funcall(rb_cYarvIBuilder, rb_intern("new"), 0);

  VALUE *iseq_original = rb_iseq_original_iseq((rb_iseq_t *)iseq);

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

    VALUE insn_object = rb_funcall(instruction_builder, rb_intern("build"), 2, rb_str_new2(insn_name(insn)), operands);
    rb_ary_push(instructions, insn_object);
  }
  return instructions;
}

VALUE yarv_builder_params(rb_iseq_t *iseq, st_table *labels_table) {
  VALUE params = rb_hash_new();

  if (iseq->body->param.flags.has_lead) {
    rb_hash_aset(params, ID2SYM(rb_intern("lead_num")), INT2FIX(iseq->body->param.lead_num));
  }

  if (iseq->body->param.flags.has_opt) {
    VALUE opts = rb_ary_new2(iseq->body->param.opt_num);
    for (int i = 0; i < iseq->body->param.opt_num; i++) {
      VALUE value = iseq->body->param.opt_table[i];
      register_label(labels_table, value);
      rb_ary_push(opts, value);
    }
    rb_hash_aset(params, ID2SYM(rb_intern("opt")), opts);
  }

  if (iseq->body->param.flags.has_rest) {
    rb_hash_aset(params, ID2SYM(rb_intern("rest_start")), INT2FIX(iseq->body->param.rest_start));
  }

  if (iseq->body->param.flags.has_post) {
    rb_hash_aset(params, ID2SYM(rb_intern("post_start")), INT2FIX(iseq->body->param.post_start));
    rb_hash_aset(params, ID2SYM(rb_intern("post_num")), INT2FIX(iseq->body->param.post_num));
  }

  if (iseq->body->param.flags.has_kw) {
    VALUE keywords = rb_ary_new();
    int j = 0;
    for (int i = 0; i < iseq->body->param.keyword->num; i++) {
      if (i < iseq->body->param.keyword->required_num) {
        // Require params
        rb_ary_push(keywords, ID2SYM(iseq->body->param.keyword->table[i]));
      } else {
        VALUE val = rb_ary_new2(2);
        rb_ary_push(val, ID2SYM(iseq->body->param.keyword->table[i]));
        rb_ary_push(val, iseq->body->param.keyword->default_values[j]);
        rb_ary_push(keywords, val);
        j++;
      }
    }
    rb_hash_aset(params, ID2SYM(rb_intern("kwbits")),INT2FIX(iseq->body->param.keyword->bits_start));
    rb_hash_aset(params, ID2SYM(rb_intern("keywords")), keywords);
  }

  if (iseq->body->param.flags.has_kwrest) {
    rb_hash_aset(params, ID2SYM(rb_intern("kwrest")), INT2FIX(iseq->body->param.rest_start));
  }

  if (iseq->body->param.flags.has_block) {
    rb_hash_aset(params, ID2SYM(rb_intern("block_start")), INT2FIX(iseq->body->param.block_start));
  }

  return params;
}

VALUE yarv_builder_catch_table(rb_iseq_t *iseq, st_table *labels_table) {
  VALUE catch_table = rb_ary_new();
  VALUE rb_cYarvCatchEntry = rb_path2class("YarvGenerator::CatchEntry");

  if (iseq->body->catch_table) {
    for (int i = 0; i < iseq->body->catch_table->size; i++) {
      VALUE catch_entry = rb_funcall(rb_cYarvCatchEntry, rb_intern("new"), 0);

      const struct iseq_catch_table_entry *entry = &iseq->body->catch_table->entries[i];
      VALUE type;
      switch (entry->type) {
        case CATCH_TYPE_RESCUE:
          type = ID2SYM(rb_intern("rescue")); break;
        case CATCH_TYPE_ENSURE:
          type = ID2SYM(rb_intern("ensure")); break;
        case CATCH_TYPE_RETRY:
          type = ID2SYM(rb_intern("retry"));  break;
        case CATCH_TYPE_BREAK:
          type = ID2SYM(rb_intern("break"));  break;
        case CATCH_TYPE_REDO:
          type = ID2SYM(rb_intern("redo"));   break;
        case CATCH_TYPE_NEXT:
          type = ID2SYM(rb_intern("next"));   break;
        default:
          rb_bug("unknown catch type %d", (int)entry->type);
      }
      rb_funcall(catch_entry, rb_intern("type="), 1, type);

      if (entry->iseq) {
        VALUE catch_iseq = yarv_builder_build_yarv_tree((rb_iseq_t *)rb_iseq_check(entry->iseq));
        rb_funcall(catch_entry, rb_intern("iseq="), 1, catch_iseq);
      }

      rb_funcall(catch_entry, rb_intern("catch_start="), 1, register_label(labels_table, entry->start));
      rb_funcall(catch_entry, rb_intern("catch_end="), 1, register_label(labels_table, entry->end));
      rb_funcall(catch_entry, rb_intern("catch_continue="), 1, register_label(labels_table, entry->cont));
      rb_funcall(catch_entry, rb_intern("sp="), 1, UINT2NUM(entry->sp));

      rb_ary_push(catch_table, catch_entry);
    }
  }
  return catch_table;
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
