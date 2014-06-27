/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2013 Derick Rethans                               |
   +----------------------------------------------------------------------+
   | This source file is subject to version 1.0 of the Xdebug license,    |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://xdebug.derickrethans.nl/license.php                           |
   | If you did not receive a copy of the Xdebug license and are unable   |
   | to obtain it through the world-wide-web, please send a note to       |
   | xdebug@derickrethans.nl so we can mail you a copy immediately.       |
   +----------------------------------------------------------------------+
   | Authors:  Derick Rethans <derick@xdebug.org>                         |
   +----------------------------------------------------------------------+
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hphp/runtime/ext/xdebug/xdebug_mm.h"
#include "hphp/runtime/ext/xdebug/xdebug_str.h"
#include "hphp/runtime/ext/xdebug/xdebug_xml.h"

static const char base64_table[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
};

static const char base64_pad = '=';

unsigned char *xdebug_base64_encode(const unsigned char *str, int length, int *ret_length)
{
  const unsigned char *current = str;
  unsigned char *p;
  unsigned char *result;

  if (length < 0) {
    if (ret_length != NULL) {
      *ret_length = 0;
    }
    return NULL;
  }

  result = (unsigned char *) malloc(((length + 2) / 3) * 4 * sizeof(char) + 1);
  p = result;

  while (length > 2) { /* keep going until we have less than 24 bits */
    *p++ = base64_table[current[0] >> 2];
    *p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
    *p++ = base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
    *p++ = base64_table[current[2] & 0x3f];

    current += 3;
    length -= 3; /* we just handle 3 octets of data */
  }

  /* now deal with the tail end of things */
  if (length != 0) {
    *p++ = base64_table[current[0] >> 2];
    if (length > 1) {
      *p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
      *p++ = base64_table[(current[1] & 0x0f) << 2];
      *p++ = base64_pad;
    } else {
      *p++ = base64_table[(current[0] & 0x03) << 4];
      *p++ = base64_pad;
      *p++ = base64_pad;
    }
  }
  if (ret_length != NULL) {
    *ret_length = (int)(p - result);
  }
  *p = '\0';
  return result;
}


static inline char *
php_memnstr(char *haystack, char *needle, int needle_len, char *end)
{
  char *p = haystack;
  char ne = needle[needle_len-1];

  if (needle_len == 1) {
    return (char *)memchr(p, *needle, (end-p));
  }

  if (needle_len > end-haystack) {
    return NULL;
  }

  end -= needle_len;

  while (p <= end) {
    if ((p = (char *)memchr(p, *needle, (end-p+1))) && ne == p[needle_len-1]) {
      if (!memcmp(needle, p, needle_len-1)) {
        return p;
      }
    }

    if (p == NULL) {
      return NULL;
    }

    p++;
  }

  return NULL;
}

char *php_str_to_str(char *haystack, int length,
  char *needle, int needle_len, char *str, int str_len, int *_new_length)
{
  char *new_str;

  if (needle_len < length) {
    char *end, *haystack_dup = NULL, *needle_dup = NULL;
    char *e, *s, *p, *r;

    if (needle_len == str_len) {
      new_str = strndup(haystack, length);
      *_new_length = length;

        end = new_str + length;
        for (p = new_str; (r = php_memnstr(p, needle, needle_len, end)); p = r + needle_len) {
          memcpy(r, str, str_len);
        }
      
      return new_str;
    } else {

      if (str_len < needle_len) {
        new_str = (char*) malloc(length + 1);
      } else {
        int count = 0;
        char *o, *n, *endp;

          o = haystack;
          n = needle;
        endp = o + length;

        while ((o = php_memnstr(o, n, needle_len, endp))) {
          o += needle_len;
          count++;
        }
        if (count == 0) {
          /* Needle doesn't occur, shortcircuit the actual replacement. */
          if (haystack_dup) {
            free(haystack_dup);
          }
          if (needle_dup) {
            free(needle_dup);
          }
          new_str = strndup(haystack, length);
          if (_new_length) {
            *_new_length = length;
          }
          return new_str;
        } else {
          new_str = (char*) malloc(count * (str_len - needle_len) + length + 1);
        }
      }

      e = s = new_str;

        end = haystack + length;
        for (p = haystack; (r = php_memnstr(p, needle, needle_len, end)); p = r + needle_len) {
          memcpy(e, p, r - p);
          e += r - p;
          memcpy(e, str, str_len);
          e += str_len;
        }

        if (p < end) {
          memcpy(e, p, end - p);
          e += end - p;
        }
      

      if (haystack_dup) {
        free(haystack_dup);
      }
      if (needle_dup) {
        free(needle_dup);
      }

      *e = '\0';
      *_new_length = e - s;

      new_str = (char*) realloc(new_str, *_new_length + 1);
      return new_str;
    }
  } else if (needle_len > length) {
nothing_todo:
    *_new_length = length;
    new_str = strndup(haystack, length);
    return new_str;
  } else {
    if (memcmp(haystack, needle, length)) {
      goto nothing_todo;
    } 
    *_new_length = str_len;
    new_str = strndup(str, str_len);

    return new_str;
  }

}

char* xdebug_xmlize(char *string, int len, int *newlen)
{
  char *tmp;
  char *tmp2;

  if (len) {
    tmp = php_str_to_str(string, len, "&", 1, "&amp;", 5, &len);

    tmp2 = php_str_to_str(tmp, len, ">", 1, "&gt;", 4, &len);
    free(tmp);

    tmp = php_str_to_str(tmp2, len, "<", 1, "&lt;", 4, &len);
    free(tmp2);

    tmp2 = php_str_to_str(tmp, len, "\"", 1, "&quot;", 6, &len);
    free(tmp);

    tmp = php_str_to_str(tmp2, len, "'", 1, "&#39;", 5, &len);
    free(tmp2);

    tmp2 = php_str_to_str(tmp, len, "\n", 1, "&#10;", 5, &len);
    free(tmp);

    tmp = php_str_to_str(tmp2, len, "\r", 1, "&#13;", 5, &len);
    free(tmp2);

    tmp2 = php_str_to_str(tmp, len, "\0", 1, "&#0;", 4, newlen);
    free(tmp);
    return tmp2;
  } else {
    *newlen = len;
    return strdup(string);
  }
}


static void xdebug_xml_return_attribute(xdebug_xml_attribute* attr, xdebug_str* output)
{
	char *tmp;
	int newlen;

	xdebug_str_addl(output, " ", 1, 0);

	/* attribute name */
	tmp = xdebug_xmlize(attr->name, attr->name_len, &newlen);
	xdebug_str_addl(output, tmp, newlen, 0);
	free(tmp);

	/* attribute value */
	xdebug_str_addl(output, "=\"", 2, 0);
	if (attr->value) {
		tmp = xdebug_xmlize(attr->value, attr->value_len, &newlen);
		xdebug_str_add(output, tmp, 0);
		free(tmp);
	}
	xdebug_str_addl(output, "\"", 1, 0);
	
	if (attr->next) {
		xdebug_xml_return_attribute(attr->next, output);
	}
}

static void xdebug_xml_return_text_node(xdebug_xml_text_node* node, xdebug_str* output)
{
	xdebug_str_addl(output, "<![CDATA[", 9, 0);
	if (node->encode) {
		/* if cdata tags are in the text, then we must base64 encode */
		int new_len = 0;
		char *encoded_text;
		
		encoded_text = (char*) xdebug_base64_encode((unsigned char*) node->text, node->text_len, &new_len);
		xdebug_str_add(output, encoded_text, 0);
		free(encoded_text);
	} else {
		xdebug_str_add(output, node->text, 0);
	}
	xdebug_str_addl(output, "]]>", 3, 0);
}

void xdebug_xml_return_node(xdebug_xml_node* node, struct xdebug_str *output)
{
	xdebug_str_addl(output, "<", 1, 0);
	xdebug_str_add(output, node->tag, 0);

	if (node->text && node->text->encode) {
		xdebug_xml_add_attribute_ex(node, "encoding", "base64", 0, 0);
	}
	if (node->attribute) {
		xdebug_xml_return_attribute(node->attribute, output);
	}
	xdebug_str_addl(output, ">", 1, 0);

	if (node->child) {
		xdebug_xml_return_node(node->child, output);
	}

	if (node->text) {
		xdebug_xml_return_text_node(node->text, output);
	}

	xdebug_str_addl(output, "</", 2, 0);
	xdebug_str_add(output, node->tag, 0);
	xdebug_str_addl(output, ">", 1, 0);

	if (node->next) {
		xdebug_xml_return_node(node->next, output);
	}
}

xdebug_xml_node *xdebug_xml_node_init_ex(char *tag, int free_tag)
{
	xdebug_xml_node *xml = (xdebug_xml_node*) xdmalloc(sizeof (xdebug_xml_node));

	xml->tag = tag;
	xml->text = NULL;
	xml->child = NULL;
	xml->attribute = NULL;
	xml->next = NULL;
	xml->free_tag = free_tag;

	return xml;
}

void xdebug_xml_add_attribute_exl(xdebug_xml_node* xml, char *attribute, size_t attribute_len, char *value, size_t value_len, int free_name, int free_value)
{
	xdebug_xml_attribute *attr = (xdebug_xml_attribute*) xdmalloc(sizeof (xdebug_xml_attribute));
	xdebug_xml_attribute **ptr;

	/* Init structure */
	attr->name = attribute;
	attr->value = value;
	attr->name_len = attribute_len;
	attr->value_len = value_len;
	attr->next = NULL;
	attr->free_name = free_name;
	attr->free_value = free_value;

	/* Find last attribute in node */
	ptr = &xml->attribute;
	while (*ptr != NULL) {
		ptr = &(*ptr)->next;
	}
	*ptr = attr;
}

void xdebug_xml_add_child(xdebug_xml_node *xml, xdebug_xml_node *child)
{
	xdebug_xml_node **ptr;

	ptr = &xml->child;
	while (*ptr != NULL) {
		ptr = &((*ptr)->next);
	}
	*ptr = child;
}

static void xdebug_xml_text_node_dtor(xdebug_xml_text_node* node)
{
	if (node->free_value && node->text) {
		xdfree(node->text);
	}
	xdfree(node);
}

void xdebug_xml_add_text(xdebug_xml_node *xml, char *text)
{
	xdebug_xml_add_text_ex(xml, text, strlen(text), 1, 0);
}

void xdebug_xml_add_text_encode(xdebug_xml_node *xml, char *text)
{
	xdebug_xml_add_text_ex(xml, text, strlen(text), 1, 1);
}

void xdebug_xml_add_text_ex(xdebug_xml_node *xml, char *text, int length, int free_text, int encode)
{
	xdebug_xml_text_node *node = (xdebug_xml_text_node*) xdmalloc(sizeof (xdebug_xml_text_node));
	node->free_value = free_text;
	node->encode = encode;
	
	if (xml->text) {
		xdebug_xml_text_node_dtor(xml->text);
	}
	node->text = text;
	node->text_len = length;
	xml->text = node;
	if (!encode && strstr(node->text, "]]>")) {
		node->encode = 1;
	}
}

static void xdebug_xml_attribute_dtor(xdebug_xml_attribute *attr)
{
	if (attr->next) {
		xdebug_xml_attribute_dtor(attr->next);
	}
	if (attr->free_name) {
		xdfree(attr->name);
	}
	if (attr->free_value) {
		xdfree(attr->value);
	}
	xdfree(attr);
}

void xdebug_xml_node_dtor(xdebug_xml_node* xml)
{
	if (xml->next) {
		xdebug_xml_node_dtor(xml->next);
	}
	if (xml->child) {
		xdebug_xml_node_dtor(xml->child);
	}
	if (xml->attribute) {
		xdebug_xml_attribute_dtor(xml->attribute);
	}
	if (xml->free_tag) {
		xdfree(xml->tag);
	}
	if (xml->text) {
		xdebug_xml_text_node_dtor(xml->text);
	}
	xdfree(xml);
}
