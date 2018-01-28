Notes on WebIDL for zephyr.js documentation:

The WebIDL fragments are for reference only; the description of the
API should be considered correct -- please report discrepancies as
soon as possible.

Although both WebIDL and Javascript are aimed at web applications,
numerous incompatibilities exist between the two.  In general, we try
to use basic (as opposed to "advanced") WebIDL to describe each
API; below, we list some of our conventions.

We map WebIDL definitions to Javascript objects as follows:

1. dictionaries -- these map to Javascript objects that simply don't
   have methods attached to them.
2. interfaces -- like definitions, these correspond to Javascript
   objects, except that they may also include methods.
3. callbacks -- these are type definitions for functions used as
   parameters or object fields.
4. enumerations -- these are largely the same in both languages.

Unless a constructor is explicitly defined/denied (*e.g.*, see the
note about "require", below), we assume the Javascript model that the
object can be constructed with "new" -- WebIDL specifies that the
external attribute "constructor" be applied to a definition that can
be new'd, but putting the attribute on every object would be
cumbersome.

The node.js notion of "require" is subtly different from constructing
an object with Javascript's "new", but it has appealing semantics.  We
annotate WebIDL definitions that should be implemented with node.js's
"require" semantics with the external attribute "ReturnFromRequire".

