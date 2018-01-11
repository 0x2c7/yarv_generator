lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'yarv_generator/version'

Gem::Specification.new do |spec|
  spec.name          = 'yarv_generator'
  spec.version       = YarvGenerator::VERSION
  spec.authors       = ['Minh Nguyen']
  spec.email         = ['nguyenquangminh0711@gmail.com']

  spec.summary       = 'A ruby-friendly YARV instruction generator'
  spec.description   = 'Ruby YARV instructions are too "internal" and hard to access from outside. This gem was born aiming to provide a friendly way to access YARV instructions for Ruby developers'
  spec.homepage      = 'https://github.com/nguyenquangminh0711/yarv_generator'
  spec.license       = 'MIT'

  spec.files         = `git ls-files -z`.split("\x0").reject do |f|
    f.match(%r{^(spec)/})
  end
  spec.require_paths = ['lib']

  spec.add_development_dependency 'bundler', '~> 1.16'
  spec.add_development_dependency 'rake', '~> 10.0'
  spec.add_development_dependency 'rspec', '~> 3.0'
end
