using HelixEngine;
using HelixEngine.UI;

namespace HelixSamples
{
    // The sample's script: rows cloned from a template into the list, a theme sheet on the root that the button
    // switches (styleSheets from C#).
    public class UIElementsDemo : GameBehaviour
    {
        public UITemplate rowTemplate;
        public StyleSheet darkTheme;
        public StyleSheet lightTheme;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            Element list = root.Find("list");
            if (rowTemplate != null && list != null)
                for (int i = 0; i < 3; i++)
                {
                    rowTemplate.CloneTree(list, out int first, out int added);
                    for (int k = first; k < first + added; k++)
                    {
                        Element row = list[k];
                        row.AddToClassList($"row-{i}");
                        // Each row a little further in.
                        row.style.translate = new Translate(new Length(i * 12), new Length(0));
                    }
                }
            if (darkTheme != null)
                root.styleSheets.Add(darkTheme);
            Element button = root.Find("theme");
            button?.RegisterCallback<ClickEvent>(e => Switch(root));
        }

        void Switch(Element root)
        {
            if (darkTheme == null || lightTheme == null)
                return;
            bool dark = root.styleSheets.Contains(darkTheme);
            root.styleSheets.Remove(dark ? darkTheme : lightTheme);
            root.styleSheets.Add(dark ? lightTheme : darkTheme);
        }
    }
}
