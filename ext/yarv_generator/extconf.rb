require 'mkmf'
require 'fileutils'

make_dir = FileUtils.pwd

# Creating tmp dir
working_dir = File.expand_path('../../tmp', File.dirname(__FILE__))
FileUtils.mkdir_p(working_dir)
puts "Working dir: #{working_dir}"

# Clone Ruby source code
ruby_source = File.expand_path('ruby', working_dir)
if File.exist?(ruby_source)
  puts "Ruby source code exists at '#{ruby_source}'."
else
  puts "Ruby source code not exist. Cloning at '#{ruby_source}'."
  result = system("git clone git@github.com:ruby/ruby #{ruby_source}")
  unless result
    puts 'Fail to clone ruby source code. Aborting.'
    exit
  end
end

# Checkout into current ruby version
FileUtils.cd(ruby_source)
ruby_version = "v#{RUBY_VERSION.gsub(/\./i, '_')}"
puts "Checking out into Ruby version #{RUBY_VERSION} with tag #{ruby_version}."
result = system("git checkout #{ruby_version}")
unless result
  puts "Fail to check out into Ruby version #{RUBY_VERSION}.
  Only official releases are supported. Aborting."
  exit
end

# Generate configure file
puts 'Generate configure file ...'
result = system('autoconf')
unless result
  puts 'Fail to generate configure file.
  Please sure autoconf is installed. Aborting'
  exit
end

# Run configure to setup compile-ready configurations
puts 'Runing configure ...'
result = system('./configure')
unless result
  puts 'Fail to compile ruby source code. Aborting.'
  exit
end

# Compile ruby source code
puts 'Compiling Ruby ...'
result = system('make')
unless result
  puts 'Fail to compile ruby source code. Aborting.'
  exit
end

# Go back and continue compiling gem
FileUtils.cd(make_dir)

cflags = " -I #{ruby_source} -I #{ruby_source}/include -I #{ruby_source}/.ext/include/#{RUBY_PLATFORM}"

with_cflags(RbConfig::CONFIG['CFLAGS'] + cflags) do
  create_makefile 'yarv_generator/yarv_generator'
end
