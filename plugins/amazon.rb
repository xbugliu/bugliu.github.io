module Jekyll
  class AmazonProductTag < Liquid::Tag
    def initialize(tag_name, text, tokens)
      super
      @text = text.gsub(/\s+/m, ' ').strip.split("*")
      @result = ""      

      (0..@text.length - 1).step(2).each do |index|
         asin = @text[index]
         title = @text[index+1]
         url = "http://www.amazon.cn/gp/product/#{asin}/ref=as_li_tf_tl?ie=UTF8&camp=536&creative=3200&creativeASIN=#{asin}&linkCode=as2&tag=bringmeluck-23"
         res = "<a href='#{url}' rel='external nofollow' target='_blank'>#{title}</a>"
         res += ", " if (index+2 != @text.length)
         @result += res     
      end

    end

    def render(context)
       @result    
    end
  end
end

Liquid::Template.register_tag('aproduct', Jekyll::AmazonProductTag)
