using System;
using HelixEngine;
using HelixEngine.Properties;
using HelixEngine.UI;

namespace HelixSamples
{
    // The object the form shows: fields and properties found by their names, a change it tells of itself.
    public class Character : INotifyBindablePropertyChanged
    {
        public class Stats
        {
            public int strength = 12;
            public int agility = 9;
        }

        int m_Level = 7;

        public event EventHandler<BindablePropertyChangedEventArgs> propertyChanged;

        public string name = "Aria";
        public float health = 0.65f;
        public bool hardcore;
        public Stats stats = new Stats();

        [CreateProperty]
        public int level
        {
            get => m_Level;
            set
            {
                if (m_Level == value)
                    return;
                m_Level = value;
                propertyChanged?.Invoke(this, new BindablePropertyChangedEventArgs(nameof(level)));
            }
        }

        [CreateProperty]
        public string summary => $"{name}, level {level}{(hardcore ? " (hardcore)" : "")}: STR {stats.strength}, AGI {stats.agility}";
    }

    // A binding class of the sample's: the element's text from the data source by its own rule, every update.
    public class PowerBinding : CustomBinding
    {
        protected override BindingResult Update(in BindingContext context)
        {
            if (context.dataSource is Character c && context.targetElement is Label label)
            {
                int power = c.level * 10 + c.stats.strength * 3 + c.stats.agility * 2;
                label.text = $"{power} ({(power > 150 ? "strong" : "growing")})";
                return new BindingResult(BindingStatus.Success);
            }
            return new BindingResult(BindingStatus.Failure, "no character");
        }
    }

    // The binding sample's script: the object as the page's data source, the markup's bindings on it, the bindings
    // made here beside them, a converter group of percents, and buttons that change the object.
    public class UIBindingDemo : GameBehaviour
    {
        Character m_Character = new Character();
        Label m_Log;

        void Start()
        {
            // The markup's source-to-ui-converters="percent".
            var percent = new ConverterGroup("percent");
            percent.AddConverter<float, string>((ref float v) => $"{Mathf.RoundToInt(v * 100)} %");
            ConverterGroups.RegisterConverterGroup(percent);

            Element root = GetComponent<UIView>().rootElement;
            Element page = root.Find("page");
            page.dataSource = m_Character;
            m_Log = root.Find<Label>("log");

            // Bindings made from C#: the bar to the health, the summary (a property of no setter) one way.
            root.Find<ProgressBar>("health-bar")?.SetBinding("value", new DataBinding
            {
                dataSourcePath = new PropertyPath(nameof(Character.health)),
                bindingMode = BindingMode.ToTarget,
            });
            root.Find<Label>("summary")?.SetBinding("text", new DataBinding
            {
                dataSourcePath = new PropertyPath(nameof(Character.summary)),
                bindingMode = BindingMode.ToTarget,
            });
            root.Find<Label>("power")?.SetBinding("text", new PowerBinding());

            // The fields' changes reach the object through their two way bindings; the log shows them.
            page.RegisterCallback<ChangeEvent<string>>(e => Log($"Name: {e.newValue}"));
            page.RegisterCallback<ChangeEvent<int>>(e => Log($"Changed to {e.newValue}"));
            page.RegisterCallback<ChangeEvent<bool>>(e => Log($"Hardcore: {e.newValue}"));

            root.Find<Button>("level-up")?.RegisterCallback<ClickEvent>(_ =>
            {
                m_Character.level = Math.Min(60, m_Character.level + 1);
                m_Character.stats.strength += 2;
                m_Character.stats.agility += 1;
                Log($"Level up: {m_Character.level}");
            });
            root.Find<Button>("heal")?.RegisterCallback<ClickEvent>(_ =>
            {
                m_Character.health = 1;
                Log("Healed");
            });
            root.Find<Button>("reset")?.RegisterCallback<ClickEvent>(_ =>
            {
                m_Character = new Character();
                page.dataSource = m_Character;
                Log("A new object as the data source");
            });
        }

        void Log(string text)
        {
            if (m_Log != null)
                m_Log.text = text;
        }
    }
}
