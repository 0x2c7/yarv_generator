class YarvGenerator
  class InstructionBuilder
    def build(name, operands)
      Instruction.new(name, operands)
    end
  end

  class Instruction
    attr_accessor :name, :operands, :label, :line_no

    def initialize(name, operands)
      @name = name
      @operands = operands
    end

    INSPECT_TEMPLATE = <<-INSPECT.strip
<%- unless label.nil? -%>
<Label <%= label -%>>
<%- end -%>
<%- if operands.any? { |o| o.is_a?(YarvGenerator::Iseq) } -%>
<%= name %>
  <%- operands.each_with_index do |o, index|-%>
    <%- if o.is_a?(YarvGenerator::Iseq) -%>
<%= YarvGenerator.indent(o.inspect, 1) %>
    <%- else -%>
<%= YarvGenerator.indent(o.respond_to?(:inspect) ? o.inspect : o, 1) %>
    <%- end -%>
  <%- end -%>
<%- else -%>
<%= name%> <%= operands.map { |o| o.respond_to?(:inspect) ? o.inspect : o }.join(", ") %>
<%- end -%>
    INSPECT

    def inspect
      ERB.new(INSPECT_TEMPLATE, nil, '-').result(binding)
    end
  end

  class CallInfo
    attr_accessor :mid, :flag, :orig_argc, :kw_arg

    def inspect
      "method(#{mid}, orig_argc: #{orig_argc})"
    end
  end
end
