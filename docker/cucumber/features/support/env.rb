require 'net/http'

require 'capybara/cucumber'
require 'capybara/poltergeist'
Capybara.configure do |c|
  c.javascript_driver = :poltergeist
  c.default_driver = :poltergeist
end
