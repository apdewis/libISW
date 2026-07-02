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
	argInt       argType = iota // uintptr — Dimension, Position, Pixel, enum, int, bool
	argString                  // string → C.CString
	argWidget                  // Widget
	argCallback                // CallbackFunc
	argStringList              // []string → CStringArray
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
	"editType":         argInt,
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
}

var macroRe = regexp.MustCompile(
	`^#define\s+IswArg(\w+)\(ab,\s*v\)\s+ISW_ARG\(\(ab\),\s*IswN(\w+),\s*\(v\)\)`)

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

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()

		if m := sectionRe.FindStringSubmatch(line); m != nil {
			currentSection = m[1]
			continue
		}

		m := macroRe.FindStringSubmatch(line)
		if m == nil {
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

		macros = append(macros, macro{
			method:      methodName,
			resourceStr: resourceName,
			typ:         typ,
			section:     currentSection,
		})
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
			fmt.Fprintf(w, "func (al *ArgList) %s(v uintptr) *ArgList {\n", m.method)
			fmt.Fprintf(w, "\treturn al.Add(%q, v)\n", m.resourceStr)
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

