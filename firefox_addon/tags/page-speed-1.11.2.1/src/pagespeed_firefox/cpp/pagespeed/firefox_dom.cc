/**
 * Copyright 2009 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pagespeed_firefox/cpp/pagespeed/firefox_dom.h"

#include "inIDOMUtils.h"
#include "nsCOMPtr.h"
#include "nsIDOM3Node.h"
#include "nsIDOMAttr.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMTreeWalker.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"  // for do_GetService
#include "nsStringAPI.h"

#include "base/logging.h"
#include "pagespeed/core/dom.h"

namespace {

const char* kDomUtilsContractId = "@mozilla.org/inspector/dom-utils;1";

bool GetStylePropertyByName(nsIDOMCSSStyleDeclaration* style,
                            const std::string& name,
                            std::string* property_value) {
  NS_ConvertASCIItoUTF16 ns_name(name.c_str());

  nsString value;
  nsresult rv = style->GetPropertyValue(ns_name, value);
  if (NS_FAILED(rv) || value.Length() == 0) {
    return false;
  }

  NS_ConvertUTF16toUTF8 converter(value);
  *property_value = converter.get();
  return true;
}

bool GetInlineStylePropertyByName(nsIDOMElement* element,
                                  const std::string& name,
                                  std::string* property_value) {
  nsresult rv;
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inline_style(
      do_QueryInterface(element, &rv));
  if (NS_FAILED(rv) || !inline_style) {
    return false;
  }

  nsCOMPtr<nsIDOMCSSStyleDeclaration> style;
  rv = inline_style->GetStyle(getter_AddRefs(style));
  if (NS_FAILED(rv) || !style) {
    return false;
  }

  return GetStylePropertyByName(style, name, property_value);
}

bool GetCascadedStylePropertyByName(nsIDOMElement* element,
                                    const std::string& name,
                                    std::string* property_value) {
  nsresult rv;
  nsCOMPtr<inIDOMUtils> dom_utils(do_GetService(kDomUtilsContractId, &rv));
  if (NS_FAILED(rv) || !dom_utils) {
    return false;
  }

  nsCOMPtr<nsISupportsArray> style_rules;
  rv = dom_utils->GetCSSStyleRules(element, getter_AddRefs(style_rules));
  if (NS_FAILED(rv) || !style_rules) {
    return false;
  }

  PRUint32 num_style_rules;
  rv = style_rules->Count(&num_style_rules);
  if (NS_FAILED(rv) || num_style_rules == 0) {
    return false;
  }

  for (int i = 0; i < num_style_rules; i++) {
    nsCOMPtr<nsISupports> rule_supports = style_rules->ElementAt(i);
    nsCOMPtr<nsIDOMCSSStyleRule> rule(do_QueryInterface(rule_supports, &rv));
    if (NS_FAILED(rv) || !rule) {
      continue;
    }

    nsCOMPtr<nsIDOMCSSStyleDeclaration> style;
    rv = rule->GetStyle(getter_AddRefs(style));
    if (NS_FAILED(rv) || !style) {
      continue;
    }

    if (GetStylePropertyByName(style, name, property_value)) {
      return true;
    }
  }

  return false;
}

class NodeFilter : public nsIDOMNodeFilter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMNODEFILTER

  NodeFilter() {}

 private:
  ~NodeFilter() {}
};

NS_IMPL_ISUPPORTS1(NodeFilter, nsIDOMNodeFilter)

NS_IMETHODIMP NodeFilter::AcceptNode(nsIDOMNode* node, PRInt16* _retval) {
  if (node == NULL || _retval == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node, &rv));

  if (NS_FAILED(rv)) {
    *_retval = nsIDOMNodeFilter::FILTER_SKIP;
    return NS_OK;
  }

  *_retval = nsIDOMNodeFilter::FILTER_ACCEPT;
  return NS_OK;
}

}  // namespace

namespace pagespeed {

namespace {

class FirefoxDocument : public DomDocument {
 public:
  FirefoxDocument(nsIDOMDocument* document);

  virtual std::string GetDocumentUrl() const;

  virtual std::string GetBaseUrl() const;

  virtual void Traverse(DomElementVisitor* visitor) const;

 private:
  nsCOMPtr<nsIDOMDocument> document_;

  DISALLOW_COPY_AND_ASSIGN(FirefoxDocument);
};

class FirefoxElement : public DomElement {
 public:
  FirefoxElement(nsIDOMElement* element);
  virtual DomDocument* GetContentDocument() const;
  virtual std::string GetTagName() const;
  virtual bool GetAttributeByName(const std::string& name,
                                  std::string* attr_value) const;

  Status GetActualWidth(int* out_width) const;
  Status GetActualHeight(int* out_height) const;
  Status HasWidthSpecified(bool* out_width_specified) const;
  Status HasHeightSpecified(bool* out_height_specified) const;

 private:
  bool GetClientWidthOrHeight(const std::string& name,
                              int* out_property_value) const;

  bool GetCSSPropertyByName(const std::string& name,
                            std::string* out_property_value) const;

  nsCOMPtr<nsIDOMElement> element_;

  DISALLOW_COPY_AND_ASSIGN(FirefoxElement);
};


FirefoxDocument::FirefoxDocument(nsIDOMDocument* document)
    : document_(document) {
}

std::string FirefoxDocument::GetDocumentUrl() const {
  nsresult rv;
  nsCOMPtr<nsIDOMHTMLDocument> html_document(
      do_QueryInterface(document_, &rv));
  if (!NS_FAILED(rv)) {
    nsString url;
    rv = html_document->GetURL(url);
    if (!NS_FAILED(rv)) {
      NS_ConvertUTF16toUTF8 converter(url);
      return converter.get();
    } else {
      LOG(ERROR) << "GetURL failed.";
      return "";
    }
  } else {
    LOG(ERROR) << "nsIDOMHTMLDocument query-interface failed.";
    return "";
  }
}

std::string FirefoxDocument::GetBaseUrl() const {
  nsresult rv;
  nsCOMPtr<nsIDOM3Node> dom3_node(
      do_QueryInterface(document_, &rv));
  if (!NS_FAILED(rv)) {
    nsString url;
    rv = dom3_node->GetBaseURI(url);
    if (!NS_FAILED(rv)) {
      NS_ConvertUTF16toUTF8 converter(url);
      return converter.get();
    } else {
      LOG(ERROR) << "GetBaseURI failed.";
      return "";
    }
  } else {
    LOG(ERROR) << "nsIDOM3Node query-interface failed.";
    return "";
  }
}

void FirefoxDocument::Traverse(DomElementVisitor* visitor) const {
  nsresult rv;
  nsCOMPtr<nsIDOMDocumentTraversal> traversal(
      do_QueryInterface(document_, &rv));
  if (!NS_FAILED(rv)) {
    nsCOMPtr<nsIDOMNodeFilter> filter(new NodeFilter);
    nsCOMPtr<nsIDOMTreeWalker> tree_walker;
    rv = traversal->CreateTreeWalker(document_,
                                     nsIDOMNodeFilter::SHOW_ALL,
                                     filter,
                                     PR_FALSE,
                                     getter_AddRefs(tree_walker));
    if (!NS_FAILED(rv)) {
      int count = 0;
      nsCOMPtr<nsIDOMNode> node;
      while (!NS_FAILED(tree_walker->NextNode(getter_AddRefs(node)))) {
        if (node == NULL) {
          break;
        }

        nsresult rv;
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node, &rv));
        if (NS_FAILED(rv)) {
          continue;
        }

        FirefoxElement wrapped_element(element);
        visitor->Visit(wrapped_element);
      }
    } else {
      LOG(ERROR) << "Tree Walker creation failed.";
    }
  } else {
    LOG(ERROR) << "Node Traversal creation failed.";
  }
}

FirefoxElement::FirefoxElement(nsIDOMElement* element) : element_(element) {
}

DomDocument* FirefoxElement::GetContentDocument() const {
  nsresult rv;
  nsCOMPtr<nsIDOMHTMLIFrameElement> iframe_element(
      do_QueryInterface(element_, &rv));

  if (!NS_FAILED(rv)) {
    nsCOMPtr<nsIDOMDocument> iframe_document;
    iframe_element->GetContentDocument(getter_AddRefs(iframe_document));
    return new FirefoxDocument(iframe_document);
  } else {
    return NULL;
  }
}

std::string FirefoxElement::GetTagName() const {
  nsString tagName;
  element_->GetTagName(tagName);

  NS_ConvertUTF16toUTF8 converter(tagName);
  return converter.get();
}

bool FirefoxElement::GetAttributeByName(const std::string& name,
                                        std::string* attr_value) const {
  nsCOMPtr<nsIDOMNamedNodeMap> attributes;
  nsresult rv = element_->GetAttributes(getter_AddRefs(attributes));
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!attributes) {
    return false;
  }

  NS_ConvertASCIItoUTF16 ns_name(name.c_str());

  nsCOMPtr<nsIDOMNode> attr_node;
  rv = attributes->GetNamedItem(ns_name, getter_AddRefs(attr_node));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIDOMAttr> attribute(do_QueryInterface(attr_node, &rv));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsString value;
  rv = attribute->GetValue(value);
  if (NS_FAILED(rv)) {
    return false;
  }

  NS_ConvertUTF16toUTF8 converter(value);
  *attr_value = converter.get();
  return true;
}

DomElement::Status FirefoxElement::GetActualWidth(int* out_width) const {
  if (!GetClientWidthOrHeight("clientWidth", out_width)) {
    return FAILURE;
  }
  return SUCCESS;
}

DomElement::Status FirefoxElement::GetActualHeight(int* out_height) const {
  if (!GetClientWidthOrHeight("clientHeight", out_height)) {
    return FAILURE;
  }
  return SUCCESS;
}

DomElement::Status FirefoxElement::HasWidthSpecified(
    bool* out_width_specified) const {
  std::string dummy;
  *out_width_specified =
      GetAttributeByName("width", &dummy) ||
      GetCSSPropertyByName("width", &dummy);
  return SUCCESS;
}

DomElement::Status FirefoxElement::HasHeightSpecified(
    bool* out_height_specified) const {
  std::string dummy;
  *out_height_specified =
      GetAttributeByName("height", &dummy) ||
      GetCSSPropertyByName("height", &dummy);
  return SUCCESS;
}

bool FirefoxElement::GetClientWidthOrHeight(const std::string& name,
                                            int* out_property_value) const {
  // TODO: generalize this to other types of nodes (not just images).
  if (name == "clientWidth" || name == "clientHeight") {
    nsresult rv;
    nsCOMPtr<nsIDOMHTMLImageElement> image(do_QueryInterface(element_, &rv));
    if (!NS_FAILED(rv)) {
      if (name == "clientWidth") {
        image->GetWidth(out_property_value);
      } else {
        image->GetHeight(out_property_value);
      }
      return true;
    }
  }

  return false;
}

bool FirefoxElement::GetCSSPropertyByName(const std::string& name,
                                          std::string* property_value) const {
  // First check inline styles, since they take precedence.
  if (GetInlineStylePropertyByName(element_, name, property_value)) {
    return true;
  }

  // Next check cascaded properties (e.g. those in a style block or
  // external stylesheet.
  return GetCascadedStylePropertyByName(element_, name, property_value);
}

}  // namespace

namespace firefox {

DomDocument* CreateDocument(nsIDOMDocument* document) {
  return new FirefoxDocument(document);
}

}  // namespace firefox

}  // namespace pagespeed
