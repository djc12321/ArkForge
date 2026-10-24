import unittest
from ui_branding import brand_text
class BrandingTest(unittest.TestCase):
    def test_visible_brands(self):
        self.assertEqual(brand_text('VS Code / VSCode / vscode / Visual Studio Code / Code - OSS'), ' / '.join(['ArkForge']*5))
    def test_protected_resources(self):
        text='[VS Code](https://vscode.dev) `vscode.workspace` command:vscode.open {0}'
        self.assertEqual(brand_text(text),'[ArkForge](https://vscode.dev) `vscode.workspace` command:vscode.open {0}')
    def test_api_identifiers(self):
        self.assertEqual(brand_text('vscode.workspace _VSCODE_NLS_LANGUAGE'), 'vscode.workspace _VSCODE_NLS_LANGUAGE')
    def test_inline_chinese(self):
        self.assertEqual(brand_text('\u6253\u5f00VSCode\u7f16\u8f91\u5668'), '\u6253\u5f00ArkForge\u7f16\u8f91\u5668')
if __name__=='__main__':unittest.main()
