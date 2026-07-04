// Command genargs parses IswArgMacros.h and generates typed ArgList methods.
// Run via go generate from the package root (see resources.go).
package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"sort"
	"strings"
	"time"
)

type argType int

const (
	argInt        argType = iota // int — Dimension, Position, int
	argString                    // string → C.CString
	argWidget                    // Widget
	argCallback                  // CallbackFunc
	argStringList                // []string → cStringArray
	argBool                      // bool
	argEnum                      // typed enum/Pixel — Go type from enumTypes
)

func (t argType) String() string {
	switch t {
	case argInt:
		return "int"
	case argString:
		return "string"
	case argWidget:
		return "widget"
	case argCallback:
		return "callback"
	case argStringList:
		return "stringlist"
	case argBool:
		return "bool"
	case argEnum:
		return "enum"
	}
	return "?"
}

var typeOverrides = map[string]argType{
	// Callbacks
	"callback":            argCallback,
	"destroyCallback":     argCallback,
	"popdownCallback":     argCallback,
	"popupCallback":       argCallback,
	"unrealizeCallback":   argCallback,
	"jumpProc":            argCallback,
	"scrollProc":          argCallback,
	"thumbProc":           argCallback,
	"notify":              argCallback,
	"valueChanged":        argCallback,
	"colorChanged":        argCallback,
	"fontChanged":         argCallback,
	"tabCallback":         argCallback,
	"startCallback":       argCallback,
	"stopCallback":        argCallback,
	"reportCallback":      argCallback,
	"exposeCallback":      argCallback,
	"resizeCallback":      argCallback,
	"inputCallback":       argCallback,
	"selectCallback":      argCallback,
	"activateCallback":    argCallback,
	"reorderCallback":     argCallback,
	"dropCallback":        argCallback,
	"dragEnterCallback":   argCallback,
	"dragMotionCallback":  argCallback,
	"dragLeaveCallback":   argCallback,
	"fileSelected":        argCallback,
	"fileCancelled":       argCallback,
	"pivotCallback":       argCallback,

	// Strings
	"editType":         argEnum,
	"accelerators":     argString,
	"label":            argString,
	"title":            argString,
	"iconName":         argString,
	"geometry":         argString,
	"string":           argString,
	"file":             argString,
	"menuName":         argString,
	"name":             argString,
	"tabLabel":         argString,
	"windowRole":       argString,
	"fontFamily":       argString,
	"previewText":      argString,
	"accelerator":      argString,
	"acceleratorText":  argString,
	"cursorName":       argString,
	"tip":              argString,
	"initialDirectory": argString,
	"templateResource": argString,
	"encoding":         argString,
	"inputMethod":      argString,
	"preeditType":      argString,
	"pivotLabel":       argString,
	"pivotImage":       argString,
	"pivotImageOpen":   argString,

	// Widgets
	"fromHoriz":    argWidget,
	"fromVert":     argWidget,
	"treeParent":   argWidget,
	"radioGroup":   argWidget,
	"topWidget":    argWidget,
	"transientFor": argWidget,
	"clientLeader": argWidget,

	// String lists
	"list":        argStringList,
	"iconLabels":  argStringList,
	"fileFilters": argStringList,

	// Booleans
	"allowShellResize":  argBool,
	"state":             argBool,
	"sensitive":         argBool,
	"showGrip":          argBool,
	"allowResize":       argBool,
	"verticalList":      argBool,
	"forceColumns":      argBool,
	"showValue":         argBool,
	"multiSelect":       argBool,
	"traversalOn":       argBool,
	"resize":            argBool,
	"allowHoriz":        argBool,
	"allowVert":         argBool,
	"mappedWhenManaged": argBool,

	// Typed enums / pixels (Go param type in enumTypes)
	"orientation":      argEnum,
	"scrollVertical":   argEnum,
	"scrollHorizontal": argEnum,
	"wrap":             argEnum,
	"ellipsize":        argEnum,
	"justify":          argEnum,
	"top":              argEnum,
	"bottom":           argEnum,
	"left":             argEnum,
	"right":            argEnum,
	"background":       argEnum,
	"foreground":       argEnum,
	"borderColor":      argEnum,
	"activeColor":      argEnum,
}

// enumTypes maps argEnum resources to their Go parameter type.
var enumTypes = map[string]string{
	"orientation":      "Orientation",
	"editType":         "TextEditType",
	"scrollVertical":   "TextScrollMode",
	"scrollHorizontal": "TextScrollMode",
	"wrap":             "TextWrapMode",
	"ellipsize":        "Ellipsize",
	"justify":          "Justify",
	"top":              "EdgeType",
	"bottom":           "EdgeType",
	"left":             "EdgeType",
	"right":            "EdgeType",
	"background":       "Pixel",
	"foreground":       "Pixel",
	"borderColor":      "Pixel",
	"activeColor":      "Pixel",
}

var macroRe = regexp.MustCompile(
	`^#define\s+IswArg(\w+)\(ab,\s*v\)\s+ISW_ARG\(\(ab\),\s*IswN(\w+),\s*\(v\)\)`)

// aliasRe matches macros that delegate to another IswArg macro,
// e.g. #define IswArgFlexGrow(ab, v) IswArgFillWeight((ab), (v))
var aliasRe = regexp.MustCompile(
	`^#define\s+IswArg(\w+)\(ab,\s*v\)\s+IswArg(\w+)\(\(ab\),\s*\(v\)\)`)

var sectionRe = regexp.MustCompile(`^/\*\s*(.+?)\s*\*/\s*$`)

type macro struct {
	method      string // Go method name (e.g. "Label")
	resourceStr string // C resource name (e.g. "label")
	typ         argType
	section     string // comment section
}

func findHeader() string {
	cmd := exec.Command("pkg-config", "--variable=includedir", "isw")
	out, err := cmd.Output()
	if err != nil {
		return ""
	}
	dir := strings.TrimSpace(string(out))
	path := dir + "/ISW/IswArgMacros.h"
	if _, err := os.Stat(path); err == nil {
		return path
	}
	return ""
}

func main() {
	headerPath := flag.String("header", "", "path to IswArgMacros.h (default: locate via pkg-config)")
	outPath := flag.String("out", "argbuilder_gen.go", "output .go file")
	flag.Parse()

	if *headerPath == "" {
		*headerPath = findHeader()
	}
	if *headerPath == "" {
		fmt.Fprintln(os.Stderr, "cannot find IswArgMacros.h; pass -header or install isw")
		os.Exit(1)
	}

	f, err := os.Open(*headerPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "open %s: %v\n", *headerPath, err)
		os.Exit(1)
	}
	defer f.Close()

	var macros []macro
	var currentSection string
	seen := make(map[string]bool)
	byMethod := make(map[string]macro)

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()

		if m := sectionRe.FindStringSubmatch(line); m != nil {
			currentSection = m[1]
			continue
		}

		m := macroRe.FindStringSubmatch(line)
		if m == nil {
			if a := aliasRe.FindStringSubmatch(line); a != nil {
				target, ok := byMethod[a[2]]
				if !ok || seen[a[1]] {
					continue
				}
				seen[a[1]] = true
				am := macro{
					method:      a[1],
					resourceStr: target.resourceStr,
					typ:         target.typ,
					section:     currentSection,
				}
				macros = append(macros, am)
				byMethod[a[1]] = am
			}
			continue
		}

		methodName := m[1]
		resourceName := m[2]

		if seen[methodName] {
			continue
		}
		seen[methodName] = true

		typ, ok := typeOverrides[resourceName]
		if !ok {
			typ = argInt
		}

		mac := macro{
			method:      methodName,
			resourceStr: resourceName,
			typ:         typ,
			section:     currentSection,
		}
		macros = append(macros, mac)
		byMethod[methodName] = mac
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "scan: %v\n", err)
		os.Exit(1)
	}

	sort.SliceStable(macros, func(i, j int) bool {
		if macros[i].section != macros[j].section {
			return macros[i].section < macros[j].section
		}
		return false
	})

	out, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "create %s: %v\n", *outPath, err)
		os.Exit(1)
	}
	defer out.Close()

	w := bufio.NewWriter(out)

	fmt.Fprintf(w, "// Code generated by genargs from IswArgMacros.h; DO NOT EDIT.\n")
	fmt.Fprintf(w, "// Generated: %s\n\n", time.Now().UTC().Format(time.RFC3339))
	fmt.Fprintf(w, "package isw\n\n")

	lastSection := ""
	for _, m := range macros {
		if m.section != lastSection {
			fmt.Fprintf(w, "// --- %s ---\n\n", m.section)
			lastSection = m.section
		}

		switch m.typ {
		case argInt:
			fmt.Fprintf(w, "func (al *ArgList) %s(v int) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.Add(%q, v)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argBool:
			fmt.Fprintf(w, "func (al *ArgList) %s(v bool) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.AddBool(%q, v)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argEnum:
			fmt.Fprintf(w, "func (al *ArgList) %s(v %s) *ArgList {\n", m.method, enumTypes[m.resourceStr])
			fmt.Fprintf(w, "\treturn al.Add(%q, int(v))\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argString:
			fmt.Fprintf(w, "func (al *ArgList) %s(v string) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.AddString(%q, v)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argWidget:
			fmt.Fprintf(w, "func (al *ArgList) %s(v Widget) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.AddWidget(%q, v)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argCallback:
			fmt.Fprintf(w, "func (al *ArgList) %s(fn CallbackFunc) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.AddCallback(%q, fn)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")

		case argStringList:
			fmt.Fprintf(w, "func (al *ArgList) %s(v []string) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.AddStringList(%q, v)\n", m.resourceStr)
			fmt.Fprintf(w, "}\n\n")
		}
	}

	if err := w.Flush(); err != nil {
		fmt.Fprintf(os.Stderr, "write: %v\n", err)
		os.Exit(1)
	}

	fmt.Fprintf(os.Stderr, "generated %d methods from %s\n", len(macros), *headerPath)
}

