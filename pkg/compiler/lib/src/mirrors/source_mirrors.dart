// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library mirrors;

import 'dart:mirrors';
import 'dart:mirrors' as api show SourceLocation;
export 'dart:mirrors';

abstract class DeclarationSourceMirror implements DeclarationMirror {
  /// Returns `true` if the name of this declaration is generated by the
  /// provider of the mirror system.
  bool get isNameSynthetic;

  /**
   * Looks up [name] in the scope of this declaration.
   *
   * [name] may be either a single identifier, like 'foo', or of the
   * a prefixed identifier, like 'foo.bar', where 'foo' must be a prefix.
   * For methods and constructors, the scope includes the parameters. For
   * classes and typedefs, the scope includes the type variables.
   * For classes and class members, the scope includes inherited members.
   *
   * See also:
   *
   * * [Lexical Scope](https://www.dartlang.org/docs/dart-up-and-running/contents/ch02.html#ch02-lexical-scope)
   *   in Dart Up and Running.
   * * [Lexical Scoping](http://www.dartlang.org/docs/spec/latest/dart-language-specification.html#h.jb82efuudrc5)
   *   in the Dart Specification.
   */
  DeclarationMirror lookupInScope(String name);
}

/**
 * Specialized [InstanceMirror] used for reflection on constant lists.
 */
abstract class ListInstanceMirror implements InstanceMirror {
  /**
   * Returns an instance mirror of the value at [index] or throws a [RangeError]
   * if the [index] is out of bounds.
   */
  InstanceMirror getElement(int index);

  /**
   * The number of elements in the list.
   */
  int get length;
}

/**
 * Specialized [InstanceMirror] used for reflection on constant maps.
 */
abstract class MapInstanceMirror implements InstanceMirror {
  /**
   * Returns a collection containing all the keys in the map.
   */
  Iterable<String> get keys;

  /**
   * Returns an instance mirror of the value for the given key or
   * null if key is not in the map.
   */
  InstanceMirror getValue(String key);

  /**
   * The number of {key, value} pairs in the map.
   */
  int get length;
}

/**
 * Specialized [InstanceMirror] used for reflection on type constants.
 */
abstract class TypeInstanceMirror implements InstanceMirror {
  /**
   * Returns the type mirror for the type represented by the reflected type
   * constant.
   */
  TypeMirror get representedType;
}

/**
 * Specialized [InstanceMirror] used for reflection on comments as metadata.
 */
abstract class CommentInstanceMirror implements InstanceMirror {
  /**
   * The comment text as written in the source text.
   */
  String get text;

  /**
   * The comment text without the start, end, and padding text.
   *
   * For example, if [text] is [: /** Comment text. */ :] then the [trimmedText]
   * is [: Comment text. :].
   */
  String get trimmedText;

  /**
   * Is [:true:] if this comment is a documentation comment.
   *
   * That is, that the comment is either enclosed in [: /** ... */ :] or starts
   * with [: /// :].
   */
  bool get isDocComment;
}

/**
 * A library.
 */
abstract class LibrarySourceMirror
    implements DeclarationSourceMirror, LibraryMirror {
  /**
   * Returns a list of the imports and exports in this library;
   */
  List<LibraryDependencyMirror> get libraryDependencies;
}

/// A mirror on an import or export declaration.
abstract class LibraryDependencySourceMirror extends Mirror
    implements LibraryDependencyMirror {
  /// Is `true` if this dependency is an import.
  bool get isImport;

  /// Is `true` if this dependency is an export.
  bool get isExport;

  /// Returns the library mirror of the library that imports or exports the
  /// [targetLibrary].
  LibraryMirror get sourceLibrary;

  /// Returns the library mirror of the library that is imported or exported.
  LibraryMirror get targetLibrary;

  /// Returns the prefix if this is a prefixed import and `null` otherwise.
  /*String*/ get prefix;

  /// Returns the list of show/hide combinators on the import/export
  /// declaration.
  List<CombinatorMirror> get combinators;

  /// Returns the source location for this import/export declaration.
  SourceLocation get location;

  /// Returns a future that completes when the library is loaded and initates a
  /// load if one has not already happened.
  /*Future<LibraryMirror>*/ loadLibrary();
}

/// A mirror on a show/hide combinator declared on a library dependency.
abstract class CombinatorSourceMirror extends Mirror
    implements CombinatorMirror {
  /// The list of identifiers on the combinator.
  List/*<String>*/ get identifiers;

  /// Is `true` if this is a 'show' combinator.
  bool get isShow;

  /// Is `true` if this is a 'hide' combinator.
  bool get isHide;
}

/**
 * Common interface for classes, interfaces, typedefs and type variables.
 */
abstract class TypeSourceMirror implements DeclarationSourceMirror, TypeMirror {
  /// Returns `true` is this is a mirror on the void type.
  bool get isVoid;

  /// Returns `true` is this is a mirror on the dynamic type.
  bool get isDynamic;

  /// Create a type mirror on the instantiation of the declaration of this type
  /// with [typeArguments] as type arguments.
  TypeMirror createInstantiation(List<TypeMirror> typeArguments);
}

/**
 * A class or interface type.
 */
abstract class ClassSourceMirror implements TypeSourceMirror, ClassMirror {
  /**
   * Is [:true:] if this class is declared abstract.
   */
  bool get isAbstract;
}

/**
 * A formal parameter.
 */
abstract class ParameterSourceMirror implements ParameterMirror {
  /**
   * Returns [:true:] iff this parameter is an initializing formal of a
   * constructor. That is, if it is of the form [:this.x:] where [:x:] is a
   * field.
   */
  bool get isInitializingFormal;

  /**
   * Returns the initialized field, if this parameter is an initializing formal.
   */
  VariableMirror get initializedField;
}

/**
 * A [SourceLocation] describes the span of an entity in Dart source code.
 * A [SourceLocation] with a non-zero [length] should be the minimum span that
 * encloses the declaration of the mirrored entity.
 */
abstract class SourceLocation implements api.SourceLocation {
  /**
   * The 1-based line number for this source location.
   *
   * A value of 0 means that the line number is unknown.
   */
  int get line;

  /**
   * The 1-based column number for this source location.
   *
   * A value of 0 means that the column number is unknown.
   */
  int get column;

  /**
   * The 0-based character offset into the [sourceText] where this source
   * location begins.
   *
   * A value of -1 means that the offset is unknown.
   */
  int get offset;

  /**
   * The number of characters in this source location.
   *
   * A value of 0 means that the [offset] is approximate.
   */
  int get length;

  /**
   * The text of the location span.
   */
  String get text;

  /**
   * Returns the URI where the source originated.
   */
  Uri get sourceUri;

  /**
   * Returns the text of this source.
   */
  String get sourceText;
}
