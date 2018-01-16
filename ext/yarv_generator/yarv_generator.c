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

VALUE
yarv_builder_build_from_source(VALUE self, VALUE src)
{
  rb_iseq_t *iseq = rb_iseq_compile_with_option(src, rb_str_new2("<compiled>"), Qnil, INT2FIX(1), 0, Qnil);
  return yarv_builder_build_yarv_tree(iseq);
}

VALUE
yarv_builder_build_yarv_tree(rb_iseq_t *iseq)
{
  // Build instruction table lookup
  static VALUE insn_table[VM_INSTRUCTION_SIZE];
  static int insn_table_init = 0;
  if (insn_table_init == 0) {
    insn_table_init = 1;
    for (int i=0; i<VM_INSTRUCTION_SIZE; i++) {
      insn_table[i] = ID2SYM(rb_intern(insn_name(i)));
    }
  }


  VALUE rb_cYarvIseq = rb_path2class("YarvGenerator::Iseq");
  VALUE iseq_object = rb_funcall(rb_cYarvIseq, rb_intern("new"), 0);

  VALUE type = yarv_builder_insn_type(iseq);
  VALUE local_table = yarv_builder_local_table(iseq);

  rb_funcall(iseq_object, rb_intern("type="), 1, type);
  rb_funcall(iseq_object, rb_intern("local_table="), 1, local_table);

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
  return rb_ary_new();
}
void Init_yarv_generator() {
  VALUE rb_cYarvGenerator = rb_path2class("YarvGenerator");
  VALUE rb_cYarvBuilder = rb_define_class_under(rb_cYarvGenerator, "Builder", rb_cObject);
  rb_define_method(rb_cYarvBuilder, "build_from_source", &yarv_builder_build_from_source, 1);
}
