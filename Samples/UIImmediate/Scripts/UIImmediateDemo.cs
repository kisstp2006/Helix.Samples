using HelixEngine;

namespace HelixSamples
{
    // The immediate GUI from C#: OnGui draws the HUD, three windows (a laid out settings form, a scrolled list of
    // companions with their pictures, notes in a text area), a dock of icon buttons with tooltips and a spinning medal,
    // over the UI panel of the page. Each pass (Layout, the input's events, Repaint) runs the same calls.
    public class UIImmediateDemo : GameBehaviour
    {
        // The pictures: the sample's own (Tools/GenerateData.py) and CC0 art of Kenney (CREDITS.txt).
        public Texture2D star, avatar, frame;
        public Texture2D panda, penguin, owl, rabbit, narwhal, frog;
        public Texture2D gear, trophy, medal, sound, music, wrench;

        Rect m_Settings = new Rect(16, 76, 280, 10);
        Rect m_Companions = new Rect(312, 76, 340, 384);
        Rect m_Notes = new Rect(668, 76, 276, 10);
        bool m_Shadows = true, m_VSync, m_Subtitles = true;
        float m_Volume = 0.7f, m_Brightness = 0.5f;
        int m_Quality = 2, m_Accent;
        string m_Name = "Helix", m_Password = "", m_Text = "Notes typed here stay with the window.\nEnter starts a new line.";
        Vector2 m_Scroll;
        readonly int[] m_Counts = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8 };
        string m_Status = "Ready";
        int m_Page, m_NextPage;
        bool m_Music = true, m_ConsoleOpen;
        Rect m_Console = new Rect(300, 150, 380, 10);
        Vector2 m_ConsoleScroll;
        string m_Command = "";
        readonly System.Collections.Generic.List<string> m_Log = new System.Collections.Generic.List<string>();
        static readonly string[] s_Pages = { "Player", "Video", "Audio" };
        float m_Spin, m_Fps = 60;
        static readonly string[] s_Quality = { "Low", "Medium", "High", "Ultra" };
        static readonly string[] s_Accents = { "Blue", "Green", "Amber", "Rose" };
        static readonly Color[] s_AccentColors =
        {
            new Color(0.45f, 0.7f, 1f, 1f), new Color(0.5f, 0.95f, 0.6f, 1f), new Color(1f, 0.8f, 0.4f, 1f), new Color(1f, 0.55f, 0.7f, 1f),
        };
        static readonly string[] s_Names = { "Panda", "Penguin", "Owl", "Rabbit", "Narwhal", "Frog", "Star", "Avatar", "Frame", "Panda II", "Owl II", "Frog II" };

        void Update()
        {
            // A batch run (the sample's screenshot) keeps still: no spin, no frame rate.
            if (Application.isBatchMode)
                return;
            m_Spin = (m_Spin + Time.deltaTime * 90f) % 360f;
            if (Time.unscaledDeltaTime > 0)
                m_Fps = Mathf.Lerp(m_Fps, 1f / Time.unscaledDeltaTime, 0.05f);
        }

        Texture2D Picture(int i)
        {
            Texture2D[] all = { panda, penguin, owl, rabbit, narwhal, frog, star, avatar, frame };
            return all[i % all.Length];
        }

        void OnGui()
        {
            Gui.backgroundColor = s_AccentColors[m_Accent];

            // The HUD: a box with what the GUI knows this pass.
            Gui.Box(new Rect(16, 16, 928, 48), "");
            string rate = Application.isBatchMode ? "" : $"   {m_Fps:F0} fps";
            Gui.Label(new Rect(28, 20, 400, 20), $"Immediate GUI from C# and C++{rate}");
            Gui.Label(new Rect(28, 40, 440, 20), $"Status: {m_Status}");
            string tip = Gui.tooltip;
            Gui.Label(new Rect(480, 20, 456, 20), string.IsNullOrEmpty(tip) ? "Hover an icon of the dock for its tooltip." : tip);
            string focused = Gui.GetNameOfFocusedControl();
            Gui.Label(new Rect(480, 40, 456, 20), $"Keyboard focus: {(string.IsNullOrEmpty(focused) ? "none" : focused)}");

            m_Settings = GuiLayout.Window(1, m_Settings, DrawSettings, "Settings", GuiLayout.Width(280));
            m_Companions = Gui.Window(2, m_Companions, DrawCompanions, "Companions");
            m_Notes = GuiLayout.Window(3, m_Notes, DrawNotes, "Notes", GuiLayout.Width(276));
            if (m_ConsoleOpen)
                m_Console = GuiLayout.Window(4, m_Console, DrawConsole, "Console", GuiLayout.Width(380));

            DrawDock();

            // A medal turned about its center (the GUI's matrix), back to none after it.
            Matrix4x4 saved = Gui.matrix;
            var pivot = new Vector2(900, 494);
            GuiUtility.RotateAroundPivot(m_Spin, pivot);
            if (medal != null)
                Gui.DrawTexture(new Rect(pivot.x - 32, pivot.y - 32, 64, 64), medal, ScaleMode.ScaleToFit);
            Gui.matrix = saved;
        }

        // The settings in pages: the toolbar picks the page the next Layout pass lays out (a pass's calls stay those of
        // its Layout pass).
        void DrawSettings(int id)
        {
            if (Event.current.type == EventType.Layout)
                m_Page = m_NextPage;
            m_NextPage = GuiLayout.Toolbar(m_NextPage, s_Pages);
            GuiLayout.Space(4);
            switch (m_Page)
            {
                case 0:
                    Gui.SetNextControlName("Player name");
                    using (new GuiLayout.HorizontalScope())
                    {
                        GuiLayout.Label("Name", GuiLayout.Width(76));
                        m_Name = GuiLayout.TextField(m_Name, 16);
                    }
                    Gui.SetNextControlName("Password");
                    using (new GuiLayout.HorizontalScope())
                    {
                        GuiLayout.Label("Password", GuiLayout.Width(76));
                        m_Password = GuiLayout.PasswordField(m_Password, '*');
                    }
                    GuiLayout.Label("Accent");
                    m_Accent = GuiLayout.SelectionGrid(m_Accent, s_Accents, 2);
                    break;
                case 1:
                    m_Shadows = GuiLayout.Toggle(m_Shadows, "Shadows");
                    m_VSync = GuiLayout.Toggle(m_VSync, "Vertical sync");
                    using (new GuiLayout.HorizontalScope())
                    {
                        GuiLayout.Label($"Brightness {m_Brightness:F2}", GuiLayout.Width(110));
                        m_Brightness = GuiLayout.HorizontalSlider(m_Brightness, 0, 1);
                    }
                    GuiLayout.Label("Quality");
                    m_Quality = GuiLayout.Toolbar(m_Quality, s_Quality);
                    break;
                default:
                    using (new GuiLayout.HorizontalScope())
                    {
                        GuiLayout.Label($"Volume {m_Volume * 100:F0}%", GuiLayout.Width(110));
                        m_Volume = GuiLayout.HorizontalSlider(m_Volume, 0, 1);
                    }
                    m_Subtitles = GuiLayout.Toggle(m_Subtitles, "Subtitles");
                    m_Music = GuiLayout.Toggle(m_Music, "Music");
                    break;
            }
            GuiLayout.Space(6);
            using (new GuiLayout.HorizontalScope())
            {
                GuiLayout.FlexibleSpace();
                if (GuiLayout.Button("Apply", GuiLayout.Width(90)))
                {
                    m_Status = $"Applied: {m_Name}, {s_Quality[m_Quality]}, volume {m_Volume * 100:F0}%, shadows {(m_Shadows ? "on" : "off")}";
                    Debug.Log(m_Status);
                }
            }
            Gui.DragWindow(new Rect(0, 0, 10000, 22));
        }

        // The debug console: the log's messages as they come (Application.logMessageReceived), a line to log.
        void DrawConsole(int id)
        {
            m_ConsoleScroll = GuiLayout.BeginScrollView(m_ConsoleScroll, GuiLayout.Height(150));
            foreach (string line in m_Log)
                GuiLayout.Label(line);
            GuiLayout.EndScrollView();
            using (new GuiLayout.HorizontalScope())
            {
                Gui.SetNextControlName("Console line");
                m_Command = GuiLayout.TextField(m_Command);
                if (GuiLayout.Button("Log", GuiLayout.Width(50)) && m_Command.Length > 0)
                {
                    Debug.Log(m_Command);
                    m_Command = "";
                }
                if (GuiLayout.Button("Clear", GuiLayout.Width(56)))
                    m_Log.Clear();
                if (GuiLayout.Button("Close", GuiLayout.Width(56)))
                    m_ConsoleOpen = false;
            }
            Gui.DragWindow(new Rect(0, 0, 10000, 22));
        }

        void OnEnable() => Application.logMessageReceived += OnLog;

        void OnDisable() => Application.logMessageReceived -= OnLog;

        void Start() => Debug.Log("UIImmediate: OnGui of C# and C++ over a UI panel.");

        void OnLog(string condition, string stackTrace, LogType type)
        {
            m_Log.Add($"[{type}] {condition}");
            if (m_Log.Count > 200)
                m_Log.RemoveAt(0);
            // The newest line in view.
            m_ConsoleScroll.y = 1e6f;
        }

        void DrawCompanions(int id)
        {
            var view = new Rect(0, 0, 300, m_Counts.Length * 52);
            m_Scroll = Gui.BeginScrollView(new Rect(8, 26, 324, 314), m_Scroll, view);
            for (int i = 0; i < m_Counts.Length; i++)
            {
                var row = new Rect(0, i * 52, 300, 48);
                Gui.Box(row, "");
                Texture2D picture = Picture(i);
                if (picture != null)
                    Gui.DrawTexture(new Rect(row.x + 6, row.y + 4, 40, 40), picture, ScaleMode.ScaleToFit);
                Gui.Label(new Rect(row.x + 56, row.y + 6, 130, 20), s_Names[i]);
                Gui.Label(new Rect(row.x + 56, row.y + 24, 130, 20), $"Count: {m_Counts[i]}");
                if (Gui.RepeatButton(new Rect(row.x + 196, row.y + 12, 44, 24), "-") && m_Counts[i] > 0)
                    m_Counts[i]--;
                if (Gui.RepeatButton(new Rect(row.x + 248, row.y + 12, 44, 24), "+"))
                    m_Counts[i]++;
            }
            Gui.EndScrollView();
            if (Gui.Button(new Rect(8, 348, 180, 26), "Scroll to the last"))
                m_Scroll.y = 1e6f;
            Gui.DragWindow(new Rect(0, 0, 10000, 22));
        }

        void DrawNotes(int id)
        {
            Gui.SetNextControlName("Notes");
            m_Text = GuiLayout.TextArea(m_Text, GuiLayout.Height(100));
            using (new GuiLayout.HorizontalScope())
            {
                if (GuiLayout.Button("Copy"))
                {
                    GuiUtility.systemCopyBuffer = m_Text;
                    m_Status = "Notes copied to the clipboard";
                }
                if (GuiLayout.Button("Clear"))
                    m_Text = "";
            }
            Gui.DragWindow(new Rect(0, 0, 10000, 22));
        }

        // Icon buttons with tooltips along the bottom, laid out in an area.
        void DrawDock()
        {
            Texture2D[] icons = { gear, trophy, sound, music, wrench };
            string[] tips = { "Settings", "Achievements", "Sound", "Music", "The debug console" };
            GuiLayout.BeginArea(new Rect(16, 470, 380, 56), Gui.skin.box);
            using (new GuiLayout.HorizontalScope())
            {
                for (int i = 0; i < icons.Length; i++)
                    if (icons[i] != null && GuiLayout.Button(new GuiContent(icons[i], tips[i]), GuiLayout.Width(64), GuiLayout.Height(40)))
                    {
                        m_Status = $"{tips[i]} pressed";
                        if (icons[i] == wrench)
                        {
                            // The console comes to the front when it opens.
                            m_ConsoleOpen = !m_ConsoleOpen;
                            if (m_ConsoleOpen)
                                Gui.BringWindowToFront(4);
                        }
                    }
            }
            GuiLayout.EndArea();
        }
    }
}
