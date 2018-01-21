require 'yarv_generator/version'
class YarvGenerator
  def self.build_from_source(src)
    YarvGenerator::Builder.new.build_from_source(src)
  end
end
require 'yarv_generator/iseq'
require 'yarv_generator/instruction'
require 'yarv_generator/yarv_generator'

