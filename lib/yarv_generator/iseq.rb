class YarvGenerator
  class Iseq
    attr_accessor :type, :local_table, :catch_table, :instructions, :params, :label, :path, :absolute_path, :first_lineno, :stack_max, :arg_size, :local_size
  end

  class CatchEntry
    attr_accessor :type, :iseq, :catch_start, :catch_end, :catch_continue, :sp
  end
end
