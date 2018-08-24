using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using csSPC;

namespace appSPC
{
    public partial class Form1 : Form
    {
        csSPC.CsSPC cs_spc = null;

        public Form1()
        {
            InitializeComponent();
            
            this.cs_spc = new CsSPC();
            bool ret = this.cs_spc.IsKeyError();

            this.timer1.Start();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            this.label1.Text = this.cs_spc.test(3, 5).ToString();
            bool ret = this.cs_spc.IsKeyError();
            this.label2.Text = ret.ToString();
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            this.cs_spc.Close();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            bool ret = this.cs_spc.IsKeyError();
            this.label2.Text = System.DateTime.Now.ToString()+"   "+ ret.ToString();
        }
    }  
}
