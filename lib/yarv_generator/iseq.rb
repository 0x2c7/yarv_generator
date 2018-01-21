class YarvGenerator
  class Iseq
    attr_accessor :type, :local_table, :catch_table, :instructions, :params
  end

  class Instruction
    attr_accessor :name, :operands
  end

  class CallInfo
    attr_accessor :mid, :flag, :orig_argc, :kw_arg
  end
end
