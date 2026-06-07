import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def generated_ninja_text() -> str:
    with tempfile.NamedTemporaryFile(suffix=".ninja") as out:
        subprocess.run(
            ["python3", "tools/gen_ninja.py", "--out", out.name],
            cwd=ROOT,
            check=True,
        )
        return Path(out.name).read_text(encoding="utf-8")


class GenNinjaDependencyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = generated_ninja_text()

    def test_xapi_objects_use_compiler_depfiles_instead_of_broad_header_edges(self) -> None:
        self.assertIn("rule xapi_cxx\n", self.text)
        self.assertIn("depfile = $out.d\n", self.text)
        self.assertIn("deps = gcc\n", self.text)
        self.assertNotIn(" | user/xapi/include/", self.text)

    def test_user_objects_use_compiler_depfiles_instead_of_broad_header_edges(self) -> None:
        self.assertIn("rule user_cxx_custom\n", self.text)
        self.assertIn("rule user_cc_custom\n", self.text)
        self.assertIn("rule browser_hosted_cxx\n", self.text)
        self.assertIn("out/ccompat-demo.elf.o", self.text)
        self.assertIn("-std=c11 -fno-stack-protector -fshort-wchar -nostdinc -I ./user/xapi/include -MMD -MP", self.text)
        self.assertNotIn(" | user/include/", self.text)
        self.assertNotIn(" | user/browser/", self.text)

    def test_stardust_objects_keep_compiler_depfile_flags(self) -> None:
        self.assertIn("rule stardust_cxx\n", self.text)
        self.assertIn("build out/stardustui/helloworld/obj/helloworld.o: stardust_cxx", self.text)
        self.assertIn("-Iframeworks/StardustUI/includes -DXJ380 -O0", self.text)
        self.assertIn("-Wno-write-strings -MMD -MP\n", self.text)


class GenNinjaLogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = generated_ninja_text()

    def test_compile_rules_use_bracketed_concise_descriptions(self) -> None:
        self.assertIn("description = \x1b[1;35m[ASM]\x1b[0m $in -> $out\n", self.text)
        self.assertIn("description = \x1b[1;32m[CC]\x1b[0m $in -> $out\n", self.text)
        self.assertIn("description = \x1b[1;36m[CXX]\x1b[0m $in -> $out\n", self.text)
        self.assertNotIn("description = CPP $in -> $out\n", self.text)

    def test_link_and_utility_rules_keep_short_action_labels(self) -> None:
        self.assertIn("description = \x1b[1;33m[LD]\x1b[0m $out\n", self.text)
        self.assertIn("description = \x1b[1;34m[CP]\x1b[0m $in -> $out\n", self.text)
        self.assertIn("description = \x1b[1;35m[GEN]\x1b[0m build.ninja\n", self.text)
        self.assertNotIn("description = LINK $out\n", self.text)
        self.assertNotIn("LINK browser objects", self.text)


if __name__ == "__main__":
    unittest.main()
