using HelixEngine;
using HelixEngine.UI;

namespace HelixSamples
{
    // An element type of a C# class: <StatCard accent="#4f9dff" tilt="-2" highlight="true">. It makes its own parts
    // (a colored strip and a body); the markup's children go into the body, its content container.
    [HxmlElement]
    public class StatCard : Element
    {
        readonly Element m_Strip = new Element { name = "strip" };
        readonly Element m_Body = new Element { name = "body" };
        Color m_Accent = new Color(0.31f, 0.62f, 1f, 1f);

        [HxmlAttribute]
        public Color accent
        {
            get => m_Accent;
            set
            {
                m_Accent = value;
                m_Strip.style.backgroundColor = value;
            }
        }

        // Degrees the card leans.
        [HxmlAttribute]
        public float tilt
        {
            get => style.rotate.value.angle.ToDegrees();
            set => style.rotate = new Rotate(new Angle(value));
        }

        [HxmlAttribute]
        public bool highlight
        {
            get => ClassListContains("highlight");
            set => EnableInClassList("highlight", value);
        }

        public override Element contentContainer => m_Body;

        public StatCard()
        {
            AddToClassList("stat-card");
            m_Strip.AddToClassList("stat-strip");
            m_Body.AddToClassList("stat-body");
            hierarchy.Add(m_Strip);
            hierarchy.Add(m_Body);
            accent = m_Accent;
        }
    }
}
