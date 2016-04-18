Given(/^a running instance of Quilt$/) do
  # Try to just open a TCP connection and wait a bit
  http = Net::HTTP.new('quilt', 80)
  http.read_timeout = 120 # Wait for 120 seconds
  http.start()
  ok = http.started?()
  http.finish()
  
  # Was it ok ?
  expect(ok).to be(true)
end

When(/^we browse to the path "([^"]*)"$/) do |path|
  # Try to visit the path
  visit('http://quilt/' + path)
end

Then(/^a page with the title "([^"]*)" shows up$/) do |title|
  expect(find(:xpath, '//body/article/h1')).to have_content(title)
end

