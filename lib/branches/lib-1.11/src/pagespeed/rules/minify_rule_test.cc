// Copyright 2009 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "pagespeed/l10n/l10n.h"
#include "pagespeed/rules/minify_rule.h"
#include "pagespeed/testing/pagespeed_test.h"

using pagespeed::Resource;
using pagespeed::UserFacingString;
using pagespeed::rules::MinifierOutput;

namespace {

// Replace all resources with a tiny plain-text file.  This would certainly
// make the web much faster, if less useful.
class FoobarMinifier : public pagespeed::rules::Minifier {
 public:
  FoobarMinifier() {}
  virtual ~FoobarMinifier() {}

  // Minifier interface:
  virtual const char* name() const { return "FoobarRule"; }
  virtual UserFacingString header_format() const {
    return not_localized("Test rule");
  }
  virtual UserFacingString body_format() const {
    return not_localized("$1 $2");
  }
  virtual UserFacingString child_format() const {
    return not_localized("$1 $2 $3");
  }
  virtual const MinifierOutput* Minify(const Resource& resource) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FoobarMinifier);
};

const MinifierOutput* FoobarMinifier::Minify(const Resource& resource) const {
  const std::string minified = "foobar";
  const int savings = resource.GetResponseBody().size() - minified.size();
  return new MinifierOutput(savings, minified, "text/plain");
}

class FoobarRule : public pagespeed::rules::MinifyRule {
 public:
  FoobarRule() : pagespeed::rules::MinifyRule(new FoobarMinifier()) {}
  virtual ~FoobarRule() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(FoobarRule);
};

class MinifyTest : public pagespeed_testing::PagespeedRuleTest<FoobarRule> {
 protected:
  void AddTestResource(const std::string &url,
                       const std::string &body) {
    Resource* resource = new Resource;
    resource->SetRequestUrl(url);
    resource->SetRequestMethod("GET");
    resource->SetResponseStatusCode(200);
    resource->SetResponseBody(body);
    AddResource(resource);
  }
};

TEST_F(MinifyTest, NoProblems) {
  AddTestResource("http://www.example.com/foo.txt",
                  "foo");
  CheckNoViolations();
}

TEST_F(MinifyTest, Unminified) {
  AddTestResource("http://www.example.com/foobarbaz.txt",
                  "foo bar baz");
  CheckOneUrlViolation("http://www.example.com/foobarbaz.txt");

  // Check that associated_result_id gets set properly.
  pagespeed::FormattedResults formatted_results;
  FormatResultsAsProto(&formatted_results);
  const pagespeed::FormattedUrlResult& url_result =
      formatted_results.rule_results(0).url_blocks(0).urls(0);
  const pagespeed::Result& res = result(0);
  ASSERT_EQ(res.id(), url_result.associated_result_id());
}

TEST_F(MinifyTest, TwoResources) {
  AddTestResource("http://www.example.com/foo.txt",
                  "foo bar baz");
  AddTestResource("http://www.example.com/blah.txt",
                  "blah blah blah");
  CheckTwoUrlViolations("http://www.example.com/foo.txt",
                        "http://www.example.com/blah.txt");

  // Check that associated_result_id is different for each resource.
  pagespeed::FormattedResults formatted_results;
  FormatResultsAsProto(&formatted_results);
  const pagespeed::FormattedUrlResult& url_result1 =
      formatted_results.rule_results(0).url_blocks(0).urls(0);
  const pagespeed::FormattedUrlResult& url_result2 =
      formatted_results.rule_results(0).url_blocks(0).urls(1);
  ASSERT_NE(url_result1.associated_result_id(),
            url_result2.associated_result_id());
}

}  // namespace
