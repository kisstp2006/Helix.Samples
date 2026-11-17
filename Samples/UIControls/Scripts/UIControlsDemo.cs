using System.Collections.Generic;
using HelixEngine;
using HelixEngine.UI;

namespace HelixSamples
{
    // The controls sample's script: controls made from C# beside the markup's, the last change shown in the status
    // line, the slider driving the progress bar, and a panel of world space settings placed in the scene (clicked and
    // typed into through the main camera's rays).
    public class UIControlsDemo : GameBehaviour
    {
        public UITemplate worldTemplate;
        Label m_Status;
        int m_Clicks, m_WorldClicks;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            m_Status = root.Find<Label>("status");
            var progress = root.Find<ProgressBar>("progress");
            root.Find<Slider>("slider")?.RegisterValueChangedCallback(e =>
            {
                if (progress != null)
                    progress.value = e.newValue;
                Status($"Slider: {e.newValue:0}");
            });
            root.Find<Toggle>("toggle")?.RegisterValueChangedCallback(e => Status($"Toggle: {e.newValue}"));
            root.Find<Button>("button").clicked += () => Status($"Button clicked {++m_Clicks}x");
            root.Find<RepeatButton>("repeat").clicked += () => Status($"Repeat {++m_Clicks}");

            // The C# side's own controls.
            Element group = root.Find("from-csharp");
            if (group != null)
            {
                var size = new PopupField<int>("Size", new List<int> { 8, 12, 16, 24 }, 1, v => $"{v} px", v => $"{v} pixels");
                size.RegisterValueChangedCallback(e => Status($"Size: {e.newValue}"));
                group.Add(size);
                var mode = new EnumField("Scale mode", ScaleMode.ScaleToFit);
                mode.RegisterValueChangedCallback(e => Status($"Scale mode: {e.newValue}"));
                group.Add(mode);
                var button = new Button(() => Status($"C# button {++m_Clicks}x")) { text = "C# button" };
                group.Add(button);
            }

            // A panel in the scene: its own settings asset of world space, its GameObject placing it.
            if (worldTemplate != null)
            {
                var settings = ScriptableObject.CreateInstance<UIPanelSettings>();
                settings.renderMode = PanelRenderMode.WorldSpace;
                var go = new GameObject("World UI");
                go.transform.position = new Vector3(2.05f, -0.1f, 3.2f);
                go.transform.rotation = Quaternion.Euler(0, 24, 0);
                go.transform.localScale = new Vector3(0.0045f, 0.0045f, 0.0045f);
                var view = go.AddComponent<UIView>();
                view.panelSettings = settings;
                view.worldSpaceSizeMode = WorldSpaceSizeMode.Fixed;
                view.worldSpaceSize = new Vector2(300, 250);
                view.source = worldTemplate;
                Element world = view.rootElement;
                var count = world.Find<Label>("world-count");
                world.Find<Button>("world-button").clicked += () =>
                {
                    m_WorldClicks++;
                    if (count != null)
                        count.text = $"{m_WorldClicks} clicks";
                    Status($"World button {m_WorldClicks}x");
                };
                world.Find<TextField>("world-text")?.RegisterValueChangedCallback(e => Status($"World text: {e.newValue}"));
            }
        }

        void Status(string text)
        {
            if (m_Status != null)
                m_Status.text = text;
        }
    }
}
