// Copyright (c) 2015, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library test.services.completion.contributor.dart.library_member;

import 'package:analysis_server/src/provisional/completion/dart/completion_dart.dart';
import 'package:analysis_server/src/services/completion/dart/library_member_contributor.dart';
import 'package:test_reflective_loader/test_reflective_loader.dart';
import 'package:unittest/unittest.dart';

import '../../../utils.dart';
import 'completion_contributor_util.dart';

main() {
  initializeTestEnvironment();
  defineReflectiveTests(LibraryMemberContributorTest);
}

@reflectiveTest
class LibraryMemberContributorTest extends DartCompletionContributorTest {
  @override
  DartCompletionContributor createContributor() {
    return new LibraryMemberContributor();
  }

  test_libraryPrefix() async {
    // SimpleIdentifier  PrefixedIdentifier  ExpressionStatement
    addTestSource('import "dart:async" as bar; foo() {bar.^}');
    await computeSuggestions();
    assertSuggestClass('Future');
    assertNotSuggested('loadLibrary');
  }

  test_libraryPrefix2() async {
    // SimpleIdentifier  MethodInvocation  ExpressionStatement
    addTestSource('import "dart:async" as bar; foo() {bar.^ print("f")}');
    await computeSuggestions();
    assertSuggestClass('Future');
  }

  test_libraryPrefix3() async {
    // SimpleIdentifier  MethodInvocation  ExpressionStatement
    addTestSource('import "dart:async" as bar; foo() {new bar.F^ print("f")}');
    await computeSuggestions();
    assertSuggestConstructor('Future');
    assertSuggestConstructor('Future.delayed');
  }

  test_libraryPrefix_deferred() async {
    // SimpleIdentifier  PrefixedIdentifier  ExpressionStatement
    addTestSource('import "dart:async" deferred as bar; foo() {bar.^}');
    await computeSuggestions();
    assertSuggestClass('Future');
    assertSuggestFunction('loadLibrary', 'Future<dynamic>');
  }

  test_libraryPrefix_with_exports() async {
    addSource('/libA.dart', 'library libA; class A { }');
    addSource('/libB.dart', 'library libB; export "/libA.dart"; class B { }');
    addTestSource('import "/libB.dart" as foo; main() {foo.^} class C { }');
    await computeSuggestions();
    assertSuggestClass('B');
    assertSuggestClass('A');
  }

  test_PrefixedIdentifier_library() async {
    // SimpleIdentifier  PrefixedIdentifier  ExpressionStatement
    addSource(
        '/testB.dart',
        '''
        lib B;
        var T1;
        class X { }
        class Y { }''');
    addTestSource('''
        import "/testB.dart" as b;
        var T2;
        class A { }
        main() {b.^}''');
    await computeSuggestions();
    expect(replacementOffset, completionOffset);
    expect(replacementLength, 0);
    assertSuggestClass('X');
    assertSuggestClass('Y');
    assertSuggestTopLevelVar('T1', null);
    assertNotSuggested('T2');
    assertNotSuggested('Object');
    assertNotSuggested('b');
    assertNotSuggested('A');
    assertNotSuggested('==');
  }

  test_PrefixedIdentifier_library_typesOnly() async {
    // SimpleIdentifier  PrefixedIdentifier  TypeName
    addSource(
        '/testB.dart',
        '''
        lib B;
        var T1;
        class X { }
        class Y { }''');
    addTestSource('''
        import "/testB.dart" as b;
        var T2;
        class A { }
        foo(b.^ f) {}''');
    await computeSuggestions();
    expect(replacementOffset, completionOffset);
    expect(replacementLength, 0);
    assertSuggestClass('X');
    assertSuggestClass('Y');
    assertNotSuggested('T1');
    assertNotSuggested('T2');
    assertNotSuggested('Object');
    assertNotSuggested('b');
    assertNotSuggested('A');
    assertNotSuggested('==');
  }

  test_PrefixedIdentifier_library_typesOnly2() async {
    // SimpleIdentifier  PrefixedIdentifier  TypeName
    addSource(
        '/testB.dart',
        '''
        lib B;
        var T1;
        class X { }
        class Y { }''');
    addTestSource('''
        import "/testB.dart" as b;
        var T2;
        class A { }
        foo(b.^) {}''');
    await computeSuggestions();
    expect(replacementOffset, completionOffset);
    expect(replacementLength, 0);
    assertSuggestClass('X');
    assertSuggestClass('Y');
    assertNotSuggested('T1');
    assertNotSuggested('T2');
    assertNotSuggested('Object');
    assertNotSuggested('b');
    assertNotSuggested('A');
    assertNotSuggested('==');
  }

  test_PrefixedIdentifier_parameter() async {
    // SimpleIdentifier  PrefixedIdentifier  ExpressionStatement
    addSource(
        '/testB.dart',
        '''
        lib B;
        class _W {M y; var _z;}
        class X extends _W {}
        class M{}''');
    addTestSource('''
        import "/testB.dart";
        foo(X x) {x.^}''');
    await computeSuggestions();
    assertNoSuggestions();
  }

  test_PrefixedIdentifier_prefix() async {
    // SimpleIdentifier  PrefixedIdentifier  ExpressionStatement
    addSource(
        '/testA.dart',
        '''
        class A {static int bar = 10;}
        _B() {}''');
    addTestSource('''
        import "/testA.dart";
        class X {foo(){A^.bar}}''');
    await computeSuggestions();
    assertNoSuggestions();
  }
}