#encoding: utf-8
Feature: Serving static files with Quilt

Scenario Outline: Ingesting a single nquad
	Given a running instance of Quilt
	When we browse to the path "<path>"
	Then a page with the title "<title>" shows up

	Examples: Test files
		| path | title |
		| 5882 | null |
		| Iceland | null |
		