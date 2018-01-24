class YarvGenerator
  class Iseq
    attr_accessor :type, :local_table, :catch_table, :instructions, :params,
                  :label, :path, :absolute_path, :first_lineno, :stack_max,
                  :arg_size, :local_size

    INSPECT_TEMPLATE = <<-INSPECT.strip
#<Iseq
  type: <%= type %>, label: <%= label %>, path: <%= path %>:<%= first_lineno %>,
  local_table: <%= local_table.inspect %>,
  params: <%= params.inspect %>,
  <%- if catch_table.empty? -%>
  catch_table: <empty>
  <%- else -%>
  catch_table:
    <%- catch_table.each do |entry| -%>
<%= YarvGenerator.indent(entry.inspect, 2) %>
    <%- end -%>
  <%- end -%>
  <%- if instructions.empty? -%>
  instructions: <empty>
  <%- else -%>
  instructions:
    <%- instructions.each do |instruction| -%>
<%= YarvGenerator.indent(instruction.inspect, 2) %>
    <%- end -%>
  <%- end -%>
>
INSPECT

    def inspect
      ERB.new(INSPECT_TEMPLATE, nil, '-').result(binding)
    end
  end

  class CatchEntry
    attr_accessor :type, :iseq, :catch_start, :catch_end, :catch_continue, :sp

    INSPECT_TEMPLATE = <<-INSPECT.strip
#<CatchEntry
  type: <%= type %>, start: <%= catch_start %>, end: <%= catch_end %> continue: <%= catch_continue %>,
  <%- if iseq.nil? -%>
  iseq: <empty>
  <%- else -%>
  iseq:
<%= YarvGenerator.indent(iseq.inspect, 2) %>
  <%- end -%>
>
INSPECT

    def inspect
      ERB.new(INSPECT_TEMPLATE, nil, '-').result(binding)
    end
  end
end
