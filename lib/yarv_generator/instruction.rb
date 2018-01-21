class YarvGenerator
  class Instruction
    attr_accessor :name, :operands
    def initialize(name, operands)
      @name = name
      @operands = operands
    end
  end

  class CallInfo
    attr_accessor :mid, :flag, :orig_argc, :kw_arg
  end

  class InstructionBuilder
    def build(name, operands)
      Instruction.new(name, operands)
    end
  end
end
