from pathlib import Path

field = Path("app/src/main/cpp/il2cpp/FieldContract.h").read_text()
contract = Path("app/src/main/cpp/il2cpp/NativePointerContract.h").read_text()
angler = Path("app/src/main/cpp/experiments/csharp/NativeAnglerQuestGameplay.cpp").read_text()

check = field[field.index("inline const char* nativeArrayContractIssue("):field.index("inline bool nativeArrayContract(")]
assert check.index("nativeManagedPointerPlausible(array)") < check.index("array->length")
assert check.index("nativeManagedPointerPlausible(array->obj.klass)") < check.index("a.class_get_rank(array->obj.klass)")
assert "return nativeArrayContractIssue(array,element,width,maximum)==nullptr;" in field
assert "ArrayRankMismatchCanonicalClassMismatch" in check
assert "ArrayLengthAbiMismatch" in check
assert "plausibleManagedPointerValue" in contract
assert "UINT64_C(0x00FFFFFFFFFFFFFF)" in contract
assert "UINT64_C(0x0001000000000000)" in contract
update = angler[angler.index("bool updateQuests("):angler.index("void pumpOnGameThread()")]
assert update.index("nativeManagedPointerPlausible(original)") < update.index("validFieldArray(original")
assert "nativeDeclaredArrayIssue" in angler
assert "class_get_element_class" not in update
assert 'g_arrayDiagnostic = "InvalidQuestItemArrayPointer";' in update
assert "setNativeGameStage(1184);" in update
assert 'Reason=%s' in angler
print("ANGLER_CORRUPT_ARRAY_NATIVE_GUARD=PASS")
